// D:\workspace\Fanan_web_hub\src\hub-settings.js
//
// File > Settings: one window, its tabs across the top, as Studio One's Options.
//
//   Appearance          Luminance (the arrangement and the Editor, dark to bright) and Contrast
//                       (text and symbols against the dark). Seen at once, kept when let go.
//   Keyboard Shortcuts  whose keys Sonify uses: Studio One's, Cubase's, Logic's, Pro Tools',
//                       Ableton Live's, Cakewalk's or FL Studio's (hub-keys.js), with the list.
//   Network             whether remote control apps on the network can find Sonify.
//   Locations           User Data (where songs, recordings, mixdowns and autosaves go; autosave
//                       every few minutes) and Plug-ins (the folders the Browser reads).
//   Audio Setup         the audio device only: the driver and device, rate, block size and
//                       channels, and, last, releasing the device while Sonify is in the background.
//   MIDI Setup          everything MIDI: the external devices, as Studio One's Add Device
//                       (keyboards, controllers, drum kits and instruments with their model, name,
//                       channels, ports, filters, split channels, default input and MPE; control
//                       surfaces speaking Mackie Control, its Extender or HUI), every MIDI input
//                       with what kind of device it is (hardware, and virtual cables such as
//                       loopMIDI), and the MIDI clock and timecode Sonify sends.
//
// Changes apply at once: there's nothing to confirm but Close. The native side keeps them
// (settings.json in AppData\Roaming\Fanan\Sonify) and tells the page at every start.

(() => {
  "use strict";

  const { $, element } = Hub;

  let settings = null;                         // the native side's, from "settings"
  let audio = null;                            // from "audioSetup"
  let library = { folders: [], thirdParty: true };
  let tab = "appearance", locationsTab = "userdata";
  let chosenDevice = "";                       // the device picked in the list (its id)

  // ---------------------------------------------------------------- the device models
  // Sonify's own, then gear by maker, as in Fender Studio's list. A model fills in the device's
  // name and maker and its kind; MPE controllers turn MPE on, at MPE's own pitch range (48
  // semitones); a control surface speaks Mackie Control, its Extender or HUI.
  const GENERIC = [
    { model: "New Keyboard", kind: "keyboard",
      about: "A MIDI keyboard. Use it for one that isn't in the list. Its Send To port is optional." },
    { model: "New Controller", kind: "controller",
      about: "Pads, knobs or faders that send notes or MIDI controllers. Use it for one that isn't in the list." },
    { model: "New Drum Kit", kind: "drums",
      about: "An electronic drum kit's module: tracks play from its pads. Use it for one that isn't in the list." },
    { model: "New Instrument", kind: "instrument",
      about: "A synth, drum machine or sequencer with MIDI in. Send it MIDI clock or timecode to play along with Sonify." },
    { model: "New Control Surface", kind: "surface", protocol: "mackie", name: "Control Surface",
      about: "For a surface that isn't in the list: choose the protocol it speaks below." }
  ];

  // k: a keyboard, p: a controller (pads, knobs, faders), d: a drum kit, i: an instrument (MIDI in),
  // m: an MPE controller; M, X and H: a control surface speaking Mackie Control, Mackie Control
  // Extender or HUI; L, A and B: a pad grid playing Song Blocks' Live (Launchpad in its Programmer
  // mode, APC mini, APC mini MK2). "=Name" after a model: the name a new device of it gets.
  const VENDORS = {
    "Access": "i:Virus TI2|i:Virus TI Snow|i:Virus C",
    "Acorn Instruments": "k:Masterkey 25|k:Masterkey 49|k:Masterkey 61",
    "AKAI": "k:MPK mini MK3|k:MPK mini Plus|k:MPK225|k:MPK249|k:MPK261|k:MPK Road 88|k:LPK25|p:MPD218|p:MPD226|p:APC40 MKII|p:APC mini MK2|A:APC mini (Clip Launcher)=APC mini|B:APC mini MK2 (Clip Launcher)=APC mini MK2|p:MIDImix|k:Advance 49|k:Advance 61|i:MPC One|i:MPC Live II",
    "Alesis": "k:V25|k:V49|k:V61|k:VI49|k:VI61|k:Q88 MKII|i:Recital Pro|d:Nitro Max|d:Strike Pro SE",
    "Arturia": "k:KeyLab 49 mk3|k:KeyLab 61 mk3|k:KeyLab 88 MkII|k:KeyLab Essential 49 mk3|k:KeyLab Essential 61 mk3|k:MiniLab 3|k:KeyStep 37|p:BeatStep|i:KeyStep Pro|i:BeatStep Pro|i:MicroFreak|i:MiniFreak|i:MatrixBrute|i:PolyBrute",
    "Behringer": "k:UMX250|k:UMX490|k:UMX610|k:Swing|M:X-Touch|X:X-Touch Extender|M:X-Touch One|p:X-Touch Mini|p:X-Touch Compact|M:BCF2000|p:BCR2000|i:Model D|i:Crave|i:Neutron|i:TD-3|i:RD-8|i:DeepMind 12|i:Pro-1",
    "Casio": "i:CT-S1|i:Privia PX-S1100|i:Privia PX-S3100",
    "CME": "k:Xkey 25|k:Xkey 37|k:Xkey Air 25|k:Xkey Air 37|k:Xkey Air 49|k:Xkey Air 61",
    "Doepfer": "k:LMK2+|k:LMK4+|p:Pocket Control|p:Drehbank|i:Dark Time",
    "E-MU": "k:Xboard 25|k:Xboard 49|k:Xboard 61",
    "Edirol": "k:PCR-300|k:PCR-500|k:PCR-800|k:PCR-M50",
    "Elektron": "i:Digitakt II|i:Digitone II|i:Analog Rytm MKII|i:Octatrack MKII|i:Syntakt|i:Model:Samples|i:Model:Cycles",
    "ESI": "k:KeyControl 25 XT|k:KeyControl 49 XT|p:Xjam",
    "Evolution": "k:MK-225C|k:MK-249C|k:MK-361C|p:UC-33",
    "Expressive E": "m:Osmose|p:Touché",
    "Fender": "i:Tone Master Pro",
    "Frontier": "M:AlphaTrack|M:TranzPort",
    "iCON": "k:iKeyboard 4 Nano|k:iKeyboard 5 Nano|k:iKeyboard 6 Nano|k:iKeyboard 8 Nano|M:Platform M+|X:Platform X+|M:P1-M|M:QCon Pro G2|X:QCon Pro XS",
    "JLCooper": "M:MCS-3000|p:CS-32 MiniDesk|p:FaderMaster Pro",
    "Kawai": "k:VPC1|i:ES120|i:ES920",
    "Keyfax": "p:PhatBoy",
    "KORG": "k:microKEY2-25|k:microKEY2-37|k:microKEY2-49|k:microKEY2-61|k:nanoKEY2|k:nanoKEY Studio|p:nanoKONTROL2|p:nanoPAD2|k:Keystage 49|k:Keystage 61|i:minilogue xd|i:monologue|i:prologue|i:opsix|i:wavestate|i:modwave|i:multi/poly|i:Kronos|i:Nautilus|i:drumlogue|i:volca bass|i:volca beats|i:volca keys|i:volca fm",
    "Mackie": "M:Control=Mackie Control|X:Control Extender=Mackie Control Extender|H:HUI=Mackie HUI",
    "M-Audio": "k:Keystation 49 MK3|k:Keystation 61 MK3|k:Keystation 88 MK3|k:Keystation Mini 32 MK3|k:Oxygen Pro 25|k:Oxygen Pro 49|k:Oxygen Pro 61|k:Oxygen Pro Mini|k:Hammer 88 Pro",
    "Moog": "i:Subsequent 37|i:Matriarch|i:Grandmother|i:Minimoog Model D|i:Muse",
    "Native Instruments": "k:Kontrol S49 MK3|k:Kontrol S61 MK3|k:Kontrol S88 MK3|k:Komplete Kontrol A25|k:Komplete Kontrol A49|k:Komplete Kontrol A61|k:Komplete Kontrol M32|p:Maschine MK3|p:Maschine Mikro MK3|i:Maschine+",
    "Nektar": "k:Impact GX49|k:Impact GX61|k:Impact LX49+|k:Impact LX61+|k:Panorama T4|k:Panorama T6|k:SE25|k:SE49",
    "Nord": "i:Nord Stage 4|i:Nord Electro 6D|i:Nord Lead A1|i:Nord Piano 5",
    "Novation": "k:Launchkey 25 MK4|k:Launchkey 37 MK4|k:Launchkey 49 MK4|k:Launchkey 61 MK4|k:Launchkey Mini 25 MK4|k:Launchkey Mini 37 MK4|k:Launchkey 49 MK3|k:Launchkey 61 MK3|k:FLkey 37|k:FLkey 49|k:FLkey 61|k:49 SL MkIII|k:61 SL MkIII|p:Launchpad X|p:Launchpad Mini MK3|p:Launchpad Pro MK3|L:Launchpad X (Clip Launcher)=Launchpad X|L:Launchpad Mini MK3 (Clip Launcher)=Launchpad Mini MK3|L:Launchpad Pro MK3 (Clip Launcher)=Launchpad Pro MK3|p:Launch Control XL|i:Bass Station II|i:Peak|i:Summit|i:Circuit Tracks|i:Circuit Rhythm",
    "Pearl": "d:e/MERGE|d:Mimic Pro|d:e/Pro Live",
    "Peavey": "p:PC 1600X|p:StudioMix",
    "Polyend": "i:Tracker|i:Synth",
    "PreSonus": "p:ATOM|p:ATOM SQ|M:FaderPort|M:FaderPort 8|M:FaderPort 16|M:ioStation 24c",
    "Roger Linn Design": "m:LinnStrument|m:LinnStrument 128",
    "Roland": "k:A-49|k:A-88MKII|k:A-500PRO|i:FANTOM-06|i:FANTOM-07|i:FANTOM-08|i:JUNO-X|i:JUNO-DS61|i:JD-Xi|i:RD-88|i:FP-30X|i:SH-4d|i:MC-707|i:TR-8S|d:TD-17|d:TD-27|d:TD-50X",
    "ROLI": "m:Seaboard RISE 2|m:Seaboard RISE 49|m:Seaboard Block M|m:LUMI Keys Studio Edition",
    "Sensel": "m:Morph",
    "Sequential": "i:Prophet-6|i:OB-6|i:Prophet Rev2|i:Take 5|i:Pro 3|i:Trigon-6",
    "Steinberg": "p:CC121|p:CMC-PD|p:CMC-FD|p:CMC-QC|p:CMC-AI",
    "Studiologic": "k:SL88 Studio|k:SL88 Grand|k:SL73 Studio|i:Numa X Piano 88",
    "TASCAM": "M:US-2400|M:FW-1884|M:FW-1082",
    "Teenage Engineering": "i:OP-1 field|i:OP-Z|i:OP-XY",
    "Waldorf": "i:Blofeld|i:Iridium|i:Quantum MK2",
    "Yamaha": "i:MODX6+|i:MODX7+|i:MODX8+|i:MONTAGE M6|i:MONTAGE M7|i:MONTAGE M8x|i:CP73|i:CP88|i:P-125|i:MX49|i:MX61|i:reface CS|i:reface DX|i:reface YC|i:reface CP|d:DTX6K3-X"
  };

  const LETTERS = { k: ["keyboard"], p: ["controller"], d: ["drums"], i: ["instrument"], m: ["keyboard"],
                    M: ["surface", "mackie"], X: ["surface", "extender"], H: ["surface", "hui"],
                    L: ["surface", "launchpad"], A: ["surface", "apcmini"], B: ["surface", "apcmini2"] };

  const ABOUT = {
    keyboard: "A MIDI keyboard: tracks play from it once it's their input.",
    controller: "A controller with pads, knobs or faders: tracks play from it once it's their input.",
    drums: "An electronic drum kit: tracks play from its pads once it's their input.",
    instrument: "An instrument with MIDI in (and often keys of its own): play tracks from its keys, and send it MIDI clock or timecode.",
    mpe: "An MPE controller: each note has a channel of its own for pitch, pressure and slide. Sonify tells the track's instrument so, with the pitch range below.",
    mackie: "A control surface that plays Sonify: its faders set the channels' volumes, its knobs the pan (or, with Send, a send's level), its buttons record-arm, solo, mute and select, and its transport, jog wheel, Undo and Save work too. The motor faders, lights, names and meters follow Sonify. Set the surface to its Mackie Control (MCU) mode, and choose both ports.",
    extender: "Eight more strips beside a Mackie Control. Surfaces take the channels in the device list's order, eight strips each: add the one on the left first. Choose both ports.",
    hui: "A control surface in HUI mode (Pro Tools' protocol): its faders, knobs, mute, solo, arm, select and transport play Sonify, and its lights, names and meters follow it. Choose both ports.",
    launchpad: "A Launchpad (Mini MK3, X or Pro MK3) playing Song Blocks' Live: its 8 \u00d7 8 pads are the launcher's grid (a row for each block, a column for each track), its side buttons launch whole blocks, its top row stops tracks, its lowest side button stops them all, and its lights follow. Choose its MIDI ports (not its DAW ports): on Windows the second ones (MIDIIN2 / MIDIOUT2). Sonify sets it to Programmer mode.",
    apcmini: "An APC mini playing Song Blocks' Live: its 8 \u00d7 8 pads are the launcher's grid (a row for each block, a column for each track), its side buttons launch whole blocks, the buttons under the pads stop tracks, Shift stops them all, and its lights follow. Choose both ports.",
    apcmini2: "An APC mini MK2 playing Song Blocks' Live: its 8 \u00d7 8 pads are the launcher's grid (a row for each block, a column for each track) lit in the blocks' colours, its side buttons launch whole blocks, the buttons under the pads stop tracks, Shift stops them all. Choose both ports."
  };

  const KIND_NAMES = { keyboard: "Keyboard", controller: "Controller", drums: "Drum kit", instrument: "Instrument", surface: "Control surface" };
  const PROTOCOL_NAMES = { mackie: "Mackie Control", extender: "Mackie Control Extender", hui: "HUI",
                           launchpad: "Launchpad (Clip Launcher)", apcmini: "APC mini (Clip Launcher)", apcmini2: "APC mini MK2 (Clip Launcher)" };

  /** What kind of device it is, as its row says on the right: "Keyboard", "Control surface · HUI"... */
  function kindName(device) {
    if (device.kind === "surface") return `Control surface · ${PROTOCOL_NAMES[device.protocol] || PROTOCOL_NAMES.mackie}`;
    if (device.mpe) return "MPE controller";
    return KIND_NAMES[device.kind] || KIND_NAMES.keyboard;
  }

  /** Every model: {model, manufacturer, kind, mpe, protocol?, name?}, made once. */
  let models = null;
  function catalogue() {
    if (models) return models;
    models = GENERIC.map((g) => ({ ...g, manufacturer: "", mpe: false }));
    for (const [vendor, list] of Object.entries(VENDORS)) {
      for (const entry of list.split("|")) {
        const letter = entry.slice(0, 1);
        const [model, name] = entry.slice(2).split("=");
        const [kind, protocol] = LETTERS[letter] || LETTERS.k;
        models.push({ model, manufacturer: vendor, kind, mpe: letter === "m", ...(protocol ? { protocol } : {}), ...(name ? { name } : {}) });
      }
    }
    return models;
  }

  // ---------------------------------------------------------------- what kind of device a port is
  // A virtual cable (loopMIDI and the like) is one whatever plays through it; otherwise the device
  // that receives from it says, then its name (a model in the list, a control surface, a MIDI
  // interface or cable) or how it's connected (Sonify's "link": usb, bluetooth or virtual).
  const CABLE_WORDS = ["loopmidi", "loopbe", "virtualmidi", "virtual", "rtpmidi", "network session", "midi yoke", "maple", "loopback",
                       "bome", "bmt", "iac", "daw remote", "rivus", "midiberry"];
  const SURFACE_WORDS = ["mackie", "mcu", "hui", "x-touch", "faderport", "fp2", "fp8", "fp16", "platform m", "platform x", "qcon", "us-2400",
                         "fw-1884", "fw-1082", "bcf2000", "alphatrack", "tranzport", "control extender", "mcs-3000"];
  const USB_CABLE_WORDS = ["midi cable", "usb2.0-midi", "usb-midi", "um-one", "um-1"];
  const INTERFACE_WORDS = ["midi interface", "um-2", "um-3", "um-880", "midisport", "midi express", "mio", "focusrite", "scarlett",
                           "clarett", "audient", "steinberg ur", "ur22", "ur44", "ur816", "audiobox", "studio 24c", "studio 68c",
                           "studio 1810c", "studio 1824c", "umc", "komplete audio", "rubix", "motu", "babyface", "fireface", "apollo",
                           "ssl 2", "minifuse"];
  const GENERIC_WORDS = new Set(["new", "control", "controller", "pro", "mini", "key", "keys", "keyboard", "midi", "usb", "the", "one",
                                 "studio", "port", "synth", "model", "in", "out", "input", "output", "device"]);

  const escapeRegex = (text) => text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const startsWord = (text, word) => new RegExp(`(^|[^a-z0-9])${escapeRegex(word)}`).test(text);   // "umc" finds "UMC404HD"

  /** A name's words, lower case and without accents: "Launchkey MK4 49 MIDI" is launchkey, mk4, 49, midi. */
  const words = (text) => text.toLowerCase().normalize("NFD").replace(/[\u0300-\u036f]/g, "").split(/[^a-z0-9]+/).filter(Boolean);
  const modelWords = new Map();   // model -> its words, made once

  /** How much a port's name is a model's: each of the model's words in it counts, a word cut
      short ("Ext" for "Extender") or run on with a number ("MiniLab3") a little less, each one missing
      takes a little off. Its first real word must be there ("X-Touch" is "touch"; Mackie's
      "Control" has none, so no port is named after it). 0 for not named after it. */
  function matchScore(model, portWords) {
    if (!modelWords.has(model)) modelWords.set(model, words(model.model));
    const own = modelWords.get(model);
    const key = own.find((w) => w.length >= 3 && !GENERIC_WORDS.has(w)) || own.find((w) => !GENERIC_WORDS.has(w));
    if (!key) return 0;

    let credit = 0, missing = 0, keyCredit = 0;
    for (const word of own) {
      let found = portWords.includes(word) ? word.length : 0;
      if (!found)
        for (const p of portWords) {
          if (p.length >= 3 && word.startsWith(p)) found = Math.max(found, p.length - 1);              // cut short
          else if (word.length >= 3 && p.startsWith(word) && /^\d+$/.test(p.slice(word.length))) found = Math.max(found, word.length - 1);   // run on
        }
      credit += found;
      if (!found) missing++;
      if (word === key) keyCredit = found;
    }

    const keyWhole = keyCredit === key.length;
    if (!keyCredit || (!keyWhole && keyCredit < 4) || credit < 3) return 0;
    return credit * 2 - missing;
  }

  /** The model in the list a port is named after: "MPK mini 3" is an AKAI MPK mini, "Launchkey MK4 49 MIDI" a Launchkey 49. */
  function modelOfPort(portName) {
    const portWords = words(portName);
    let best = null, bestScore = 0;
    for (const model of catalogue()) {
      if (!model.manufacturer) continue;
      const score = matchScore(model, portWords);
      if (score > bestScore) { best = model; bestScore = score; }
    }
    return best;
  }

  /** What kind of device a MIDI input port is, for the right of its row. */
  function portKind(port, devices) {
    const name = port.name.toLowerCase();
    if (port.link === "virtual" || CABLE_WORDS.some((word) => startsWord(name, word))) return "Virtual MIDI cable";
    const user = devices.find((d) => d.input === port.name);
    if (user) return kindName(user);
    const model = modelOfPort(port.name);
    if (model) return kindName(model);
    if (SURFACE_WORDS.some((word) => startsWord(name, word))) return "Control surface";
    if (USB_CABLE_WORDS.some((word) => startsWord(name, word))) return "USB MIDI cable";
    if (INTERFACE_WORDS.some((word) => startsWord(name, word))) return "MIDI interface";
    return port.link === "bluetooth" ? "Bluetooth MIDI device" : "Keyboard / controller";
  }

  /** Where a MIDI source goes in a track's MIDI in list: keyboards first (0), then hardware that may
      be a keyboard or a controller (1: a MIDI interface or cable, Bluetooth, a port not known),
      then controllers, drum kits and surfaces (2), and virtual MIDI cables last (3). source: an
      External Device's name (its kind says) or a port's, with or without "|channel". */
  function inputRank(source) {
    const bar = source.lastIndexOf("|");
    const base = bar > 0 && /^\d+$/.test(source.slice(bar + 1)) ? source.slice(0, bar) : source;
    const devices = (settings && settings.devices) || [];
    const ports = (settings && settings.midiInputs) || [];
    const device = devices.find((d) => d.name === base);
    if (device && (device.kind === "keyboard" || device.kind === "instrument")) return 0;
    if (device && device.kind) return 2;
    const portName = device && device.input ? device.input : base;
    const kind = portKind(ports.find((p) => p.name === portName) || { name: portName }, devices);
    if (kind === "Virtual MIDI cable") return 3;
    if (kind === KIND_NAMES.keyboard || kind === KIND_NAMES.instrument) return 0;
    if (kind === KIND_NAMES.controller || kind === KIND_NAMES.drums || kind.startsWith("Control surface")) return 2;
    return 1;
  }

  // ---------------------------------------------------------------- opening and tabs
  function open(which, sub) {
    if (which) tab = which;
    if (sub) locationsTab = sub;
    $("settings").hidden = false;
    Hub.send("getSettings");
    showTab(tab);
    const selected = document.querySelector(`[data-settings-tab="${tab}"]`);
    if (selected) selected.focus();   // the keyboard is in the window now
  }

  function close() {
    $("dev-dialog").hidden = true;
    $("settings").hidden = true;
  }

  function showTab(name) {
    tab = name;
    for (const button of document.querySelectorAll("[data-settings-tab]")) {
      const on = button.dataset.settingsTab === name;
      button.setAttribute("aria-selected", String(on));
      $(`settings-${button.dataset.settingsTab}`).hidden = !on;
    }
    if (name === "audio") Hub.send("audioSetupGet", { rescan: true });
    render();
  }

  function render() {
    if ($("settings").hidden) return;
    if (tab === "appearance") renderAppearance();
    else if (tab === "keys") renderKeys();
    else if (tab === "network") renderNetwork();
    else if (tab === "locations") renderLocations();
    else if (tab === "audio") renderAudio();
    else if (tab === "devices") renderDevices();
  }

  const fill = (input) => input.style.setProperty("--fill", `${(Number(input.value) - Number(input.min)) / (Number(input.max) - Number(input.min)) * 100}%`);

  // ---------------------------------------------------------------- appearance
  function renderAppearance() {
    const theme = Hub.theme;
    for (const [id, value] of [["set-luminance", theme.luminance], ["set-contrast", theme.contrast]]) {
      const input = $(id);
      if (document.activeElement !== input) input.value = String(value);
      $(`${id}-value`).textContent = String(input.value);
      fill(input);
    }
  }

  // ---------------------------------------------------------------- keyboard shortcuts
  function renderKeys() {
    const select = $("set-keymap");
    if (select.options.length === 0)
      for (const map of Hub.keys.maps()) select.add(new Option(map.name, map.id));
    select.value = Hub.keys.current();

    const map = Hub.keys.maps().find((m) => m.id === select.value);
    $("set-keymap-note").textContent = map && map.note ? map.note : "Sonify's commands with the keys this DAW gives them; where it has none, there's none here either.";
    $("set-keymap-note").hidden = false;

    const list = $("set-key-list");
    list.replaceChildren();

    for (const group of Hub.keys.list(select.value)) {
      const box = element("div", "key-group");
      box.appendChild(element("div", "key-group-title", group.group));
      for (const item of group.items) {
        const row = element("div", "key-row" + (item.keys.length ? "" : " none"));
        const keys = element("span", "key-keys");
        if (item.keys.length) for (const key of item.keys) keys.appendChild(element("span", "key-cap", key));
        else keys.textContent = "\u2014";
        row.append(element("span", "key-name", item.name), keys);
        box.appendChild(row);
      }
      list.appendChild(box);
    }
  }

  // ---------------------------------------------------------------- network
  function renderNetwork() {
    const on = Boolean(settings && settings.discoverable);
    $("set-discoverable").checked = on;
    const name = settings && settings.computerName ? settings.computerName : "this computer";
    $("set-discoverable-note").textContent = on
      ? `On. Sonify announces itself on your local network as "${name}" (UDP port ${settings.discoveryPort}), so remote control apps on the same network find it.`
      : "Off. Remote control apps on your network can't find Sonify. Turn it on to use one (Windows may ask whether Sonify may use the network).";
  }

  // ---------------------------------------------------------------- locations
  function renderLocations() {
    for (const button of document.querySelectorAll("[data-locations-tab]")) {
      const on = button.dataset.locationsTab === locationsTab;
      button.setAttribute("aria-selected", String(on));
      $(`locations-${button.dataset.locationsTab}`).hidden = !on;
    }

    if (settings) {
      const path = $("set-userdata");
      path.textContent = settings.userData;
      path.title = settings.userData;
      $("set-userdata-default").disabled = settings.userDataIsDefault;
      $("set-autosave").checked = settings.autosave;
      const minutes = $("set-autosave-minutes");
      if (document.activeElement !== minutes) minutes.value = String(settings.autosaveMinutes);
      minutes.disabled = !settings.autosave;
      $("set-autosave-note").textContent = settings.autosave
        ? `While a song has unsaved changes, a copy is saved every ${settings.autosaveMinutes} ${settings.autosaveMinutes === 1 ? "minute" : "minutes"} to ${settings.autosaveFolder}. Your song itself is only saved when you save it.`
        : "Off: only saving keeps your work.";
    }

    const list = $("set-folder-list");
    list.replaceChildren();
    const ownOnly = library.thirdParty === false;

    for (const folder of library.folders || []) {
      const row = element("div", "folder-row");
      const path = element("span", "folder-path", folder);
      path.title = folder;
      const change = element("button", "plain-button small", "Change\u2026");
      change.type = "button";
      change.title = "Choose another folder in its place";
      change.disabled = ownOnly;
      change.addEventListener("click", () => Hub.send("replaceLibraryFolder", { folder }));
      const remove = element("button", "icon-button", "\u00d7");
      remove.type = "button";
      remove.title = "Remove this folder: its plugins leave the Browser";
      remove.setAttribute("aria-label", `Remove ${folder}`);
      remove.addEventListener("click", () => Hub.send("removeLibraryFolder", { folder }));
      row.append(path, change, remove);
      list.appendChild(row);
    }

    $("set-folders-empty").hidden = (library.folders || []).length > 0;
    for (const id of ["set-folders-add", "set-folders-defaults", "set-folders-rescan"]) $(id).disabled = ownOnly;

    const defaults = settings && settings.defaultPluginFolders ? settings.defaultPluginFolders : [];
    $("set-folders-note").textContent = ownOnly
      ? "Third-party plugins are off until Sonify starts again, so the folders can't change now."
      : defaults.length ? `The usual folders: ${defaults.join(" and ")}. Sonify loads VST3, CLAP and WCLAP plugins.` : "";
  }

  // ---------------------------------------------------------------- audio / MIDI
  /** Fills a select with [value, label] pairs, keeping what's chosen. */
  function options(select, pairs, chosen) {
    const signature = JSON.stringify(pairs);
    if (select.dataset.signature !== signature) {
      select.replaceChildren(...pairs.map(([value, label]) => new Option(label, String(value))));
      select.dataset.signature = signature;
    }
    select.value = String(chosen);
    if (select.selectedIndex < 0 && pairs.length) select.selectedIndex = 0;
  }

  function renderAudio() {
    const a = audio;
    $("set-release").checked = Boolean(settings && settings.releaseInBackground);
    if (!a) return;

    options($("set-audio-type"), a.types.map((t) => [t, t]), a.type);
    const separate = Boolean(a.separate);
    $("set-audio-device-row").hidden = separate;
    $("set-audio-output-row").hidden = !separate;
    $("set-audio-input-row").hidden = !separate;

    const none = [["", "None"]];
    if (separate) {
      options($("set-audio-output"), none.concat((a.outputs || []).map((d) => [d, d])), a.output || "");
      options($("set-audio-input"), none.concat((a.inputs || []).map((d) => [d, d])), a.input || "");
    } else {
      options($("set-audio-device"), none.concat((a.outputs || []).map((d) => [d, d])), a.output || "");
    }

    const open = Boolean(a.open);
    for (const id of ["set-audio-rate", "set-audio-buffer", "set-audio-test"]) $(id).disabled = !open;
    $("set-audio-panel").hidden = !a.hasControlPanel;

    if (open) {
      options($("set-audio-rate"), (a.rates || []).map((r) => [r, `${Number.isInteger(r / 1000) ? r / 1000 : (r / 1000).toFixed(1)} kHz`]), a.rate);
      options($("set-audio-buffer"), (a.buffers || []).map((b) => [b, `${b} samples (${(1000 * b / a.rate).toFixed(1)} ms)`]), a.buffer);
      $("set-audio-latency").textContent = `In ${a.inputLatencyMs.toFixed(1)} ms · Out ${a.outputLatencyMs.toFixed(1)} ms`;

      // Outputs: every channel is open; Song Setup's Outputs say where Main and the Subs play.
      const names = a.outputChannels || [];
      const main = (Hub.state.outputs || [])[0];
      const mainText = main && names[main.left] !== undefined
        ? `Main on ${names[main.left]}${main.right >= 0 && names[main.right] !== undefined ? " + " + names[main.right] : ""}`
        : "Main on the first two";
      $("set-audio-outputs").textContent = `${names.length} open · ${mainText}`;

      // Inputs: each channel on or off; audio tracks record from the ones on (Song Setup names them).
      const box = $("set-audio-inputs");
      box.replaceChildren();
      (a.inputChannels || []).forEach((name, index) => {
        const label = element("label", "channel-choice");
        const check = element("input");
        check.type = "checkbox";
        check.checked = Boolean((a.activeInputs || [])[index]);
        check.addEventListener("change", () => {
          const channels = (a.inputChannels || []).map((_, i) => (i === index ? check.checked : Boolean((a.activeInputs || [])[i])));
          Hub.send("audioSetupSet", { inputChannels: channels });
        });
        label.append(check, document.createTextNode(` ${name}`));
        box.appendChild(label);
      });
      if (!(a.inputChannels || []).length) box.appendChild(element("span", "muted small", "This device has no inputs."));
    } else {
      $("set-audio-latency").textContent = a.released ? "Let go while Sonify is in the background." : "No device open.";
      $("set-audio-inputs").replaceChildren(element("span", "muted small", "\u2013"));
      $("set-audio-outputs").textContent = "\u2013";
    }

    const error = $("set-audio-error");
    error.textContent = a.error || "";
    error.hidden = !a.error;
  }

  // ---------------------------------------------------------------- MIDI setup: external devices
  function deviceSummary(device) {
    const parts = [`${device.manufacturer ? device.manufacturer + " " : ""}${device.model}`];
    parts.push(device.input ? `from ${device.input}` : "receives nothing");
    if (device.output) parts.push(`to ${device.output}`);
    if (device.kind !== "surface") {
      if (device.channels !== 0xffff) parts.push(`channels ${channelText(device.channels)}`);
      if (device.splitChannels) parts.push("split");
      if (device.mpe) parts.push(`MPE ${device.pitchRange} st`);
    }
    return parts.join(" \u00b7 ");
  }

  function channelText(mask) {
    const list = [];
    for (let c = 1; c <= 16; c++) if (mask & (1 << (c - 1))) list.push(c);
    return list.length === 16 ? "all" : list.join(", ");
  }

  function renderDevices() {
    const devices = (settings && settings.devices) || [];
    const list = $("set-device-list");
    list.replaceChildren();
    if (!devices.some((d) => d.id === chosenDevice)) chosenDevice = "";

    for (const device of devices) {
      const row = element("div", "device-row");
      row.setAttribute("role", "option");
      row.setAttribute("aria-selected", String(device.id === chosenDevice));
      row.tabIndex = 0;

      const name = element("div", "device-row-name");
      name.append(element("span", "", device.name));
      if (device.defaultInput) name.append(element("span", "device-badge", "Default input"));
      if (device.sendClock) name.append(element("span", "device-badge", "Clock"));
      if (device.sendTimecode) name.append(element("span", "device-badge", "MTC"));

      // What kind of device it is, on the right.
      row.append(name, element("div", "device-row-kind", kindName(device)), element("div", "device-row-ports", deviceSummary(device)));
      row.addEventListener("click", () => { chosenDevice = device.id; renderDevices(); });
      row.addEventListener("dblclick", () => editDevice(device));
      row.addEventListener("keydown", (event) => { if (event.key === "Enter") editDevice(device); });
      list.appendChild(row);
    }

    $("set-devices-empty").hidden = devices.length > 0;
    $("set-device-edit").disabled = !chosenDevice;
    $("set-device-remove").disabled = !chosenDevice;

    // A control surface missing a port, or whose port is off or held by another app.
    const surfaceProblems = (settings && settings.surfaceProblems) || [];
    $("set-surface-problems").textContent = surfaceProblems.join("\n");
    $("set-surface-problems").hidden = !surfaceProblems.length;

    renderMidiInputs(devices);
    if (settings) $("set-timecode-rate").value = settings.timecodeRate;

    const problems = (settings && settings.syncProblems) || [];
    $("set-sync-problems").textContent = problems.length ? `Couldn't open ${problems.join(", ")} to send MIDI clock or timecode: another app may be using it.` : "";
    $("set-sync-problems").hidden = !problems.length;
  }

  /** MIDI inputs: every port on the computer, hardware and virtual (loopMIDI's too), ticked when
      Sonify listens to it, with the devices above that receive from it and, on the right, what kind
      of device it is. MIDI, so here and not with the audio device. */
  function renderMidiInputs(devices) {
    const ports = (settings && settings.midiInputs) || [];
    const list = $("set-midi-inputs");
    list.replaceChildren();

    for (const port of ports) {
      const row = element("label", "midi-port");
      const check = element("input");
      check.type = "checkbox";
      check.checked = Boolean(port.enabled);
      check.addEventListener("change", () => Hub.send("midiPortEnabled", { id: port.id, enabled: check.checked }));
      row.append(check, element("span", "midi-port-name", port.name));

      const users = devices.filter((d) => d.input === port.name);
      const surfaces = users.filter((d) => d.kind === "surface").map((d) => d.name);
      const players = users.filter((d) => d.kind !== "surface").map((d) => d.name);
      if (surfaces.length) {
        const tag = element("span", "midi-port-device", `Plays Sonify as ${surfaces.join(", ")}`);
        tag.title = "A control surface above receives from this port: it plays Sonify, so tracks don't hear it";
        row.appendChild(tag);
      } else if (players.length) {
        const tag = element("span", "midi-port-device", `Tracks see it as ${players.join(", ")}`);
        tag.title = "A device above receives from this port: tracks pick it by the device's name";
        row.appendChild(tag);
      }
      if (port.busy) row.appendChild(element("span", "midi-port-busy", "Can't open it: another app is using it"));
      row.appendChild(element("span", "midi-port-kind", portKind(port, devices)));
      list.appendChild(row);
    }

    list.hidden = ports.length === 0;
    $("set-midi-none").hidden = ports.length > 0;
  }

  // ---------------------------------------------------------------- the device dialog
  let editing = null;          // the device being made or changed
  let pickedModel = null;      // the model picked in the tree
  let autoInput = false;       // its Receive From is Sonify's guess, not the user's choice
  let autoOutput = false;      // and its Send To
  const openVendors = new Set();

  const inputs = () => ((settings && settings.midiInputs) || []).map((p) => p.name);
  const outputs = () => ((settings && settings.midiOutputs) || []).map((p) => p.name);

  /** A control surface plays Sonify: none of a keyboard's notes, channels, filters or sync. */
  const SURFACE_OFF = { channels: 0xffff, filterAftertouch: false, filterProgramChange: false, filterPitchBend: false, filterControllers: false,
                        splitChannels: false, defaultInput: false, mpe: false, sendClock: false, clockStart: false, sendTimecode: false };

  /** The port named after a model ("Launchkey 49 MK4" finds "Launchkey MK4 49 MIDI"), or "": one
      named after it more than after any other model ("X-Touch" isn't "X-TOUCH MINI"), and not
      one that another device already uses. */
  function portFor(model, ports, used) {
    let best = "", bestScore = 0;
    for (const port of ports) {
      if (used.has(port)) continue;
      const portWords = words(port);
      const score = matchScore(model, portWords);
      if (score <= bestScore) continue;
      const rival = catalogue().some((other) => other !== model && other.manufacturer && matchScore(other, portWords) > score);
      if (!rival) { best = port; bestScore = score; }
    }
    return best;
  }

  const othersDefault = (id) => ((settings && settings.devices) || []).some((d) => d.defaultInput && d.id !== id);

  /** The first port Sonify listens to that no device receives from yet. */
  function freeInput() {
    const used = new Set(((settings && settings.devices) || []).map((d) => d.input));
    const free = ((settings && settings.midiInputs) || []).find((p) => p.enabled && !used.has(p.name));
    return free ? free.name : "";
  }

  function newDevice() {
    return { id: "", model: "New Keyboard", kind: "keyboard", protocol: "mackie", manufacturer: "", name: "New Keyboard",
             input: freeInput(), output: "", channels: 0xffff,
             filterAftertouch: false, filterProgramChange: false, filterPitchBend: false, filterControllers: false,
             splitChannels: false, defaultInput: !othersDefault(""),
             mpe: false, pitchRange: 48, sendClock: false, clockStart: false, sendTimecode: false };
  }

  function editDevice(device) {
    editing = device ? { protocol: "mackie", ...device } : newDevice();
    autoInput = autoOutput = !device;
    pickedModel = catalogue().find((m) => m.model === editing.model && (m.manufacturer || "") === (editing.manufacturer || ""))
               || catalogue().find((m) => m.model === editing.model) || null;
    if (pickedModel && pickedModel.manufacturer) openVendors.add(pickedModel.manufacturer);
    $("dev-title").textContent = device ? `Edit Device: ${device.name}` : "Add Device";
    $("dev-search").value = "";
    $("dev-dialog").hidden = false;
    renderTree();
    renderDeviceForm();
    $("dev-name").focus();
  }

  const nameOf = (model) => (model ? model.name || model.model : "");

  function pickModel(model) {
    const was = pickedModel;
    pickedModel = model;
    const d = editing;
    // A name and maker still as the last model had them follow the new model.
    if (!d.name || (was && d.name.startsWith(nameOf(was))) || GENERIC.some((g) => d.name === g.model || d.name === g.name)) d.name = nameOf(model);
    if (!d.manufacturer || (was && d.manufacturer === was.manufacturer)) d.manufacturer = model.manufacturer;
    d.model = model.model;
    const wasSurface = d.kind === "surface";
    d.kind = model.kind;
    if (model.protocol) d.protocol = model.protocol;
    d.mpe = Boolean(model.mpe);
    if (model.mpe) d.pitchRange = 48;
    if (model.kind === "surface") Object.assign(d, SURFACE_OFF);
    else if (wasSurface) d.defaultInput = !othersDefault(d.id);   // as a new device would have it

    // Its ports, when they're named after it: a surface listens as well as sends. A guessed port
    // isn't a surface's (a keyboard's port taken by a surface would stop playing tracks).
    const others = ((settings && settings.devices) || []).filter((other) => other.id !== d.id);
    const usedInputs = new Set(others.map((other) => other.input).filter(Boolean));
    const usedOutputs = new Set(others.filter((other) => other.kind === "surface" || model.kind === "surface").map((other) => other.output).filter(Boolean));
    const input = model.manufacturer ? portFor(model, inputs(), usedInputs) : "";
    const output = model.manufacturer ? portFor(model, outputs(), usedOutputs) : "";
    if (input) d.input = input;
    else if (autoInput) d.input = model.kind === "surface" ? "" : freeInput();
    if (output && (model.kind === "instrument" || model.kind === "surface")) d.output = output;
    else if (autoOutput) d.output = "";

    renderTree();
    renderDeviceForm();
  }

  function renderTree() {
    const tree = $("dev-tree");
    const query = $("dev-search").value.trim().toLowerCase();
    tree.replaceChildren();

    const item = (model, depth) => {
      const button = element("button", "tree-item model");
      button.type = "button";
      button.setAttribute("role", "treeitem");
      button.style.paddingLeft = `${8 + depth * 14}px`;
      const selected = pickedModel && pickedModel.model === model.model && (pickedModel.manufacturer || "") === (model.manufacturer || "");
      button.setAttribute("aria-selected", String(Boolean(selected)));
      button.title = kindName(model);
      button.append(element("span", "tree-icon " + (model.mpe ? "mpe" : model.kind)), element("span", "", model.model));
      button.addEventListener("click", () => pickModel(model));
      tree.appendChild(button);
    };

    for (const model of catalogue().filter((m) => !m.manufacturer)) if (!query || model.model.toLowerCase().includes(query)) item(model, 0);

    for (const vendor of Object.keys(VENDORS).sort((a, b) => a.localeCompare(b, undefined, { sensitivity: "base" }))) {
      const models = catalogue().filter((m) => m.manufacturer === vendor && (!query || `${vendor} ${m.model}`.toLowerCase().includes(query)));
      if (!models.length) continue;
      const opened = query !== "" || openVendors.has(vendor);
      const folder = element("button", "tree-item vendor");
      folder.type = "button";
      folder.setAttribute("role", "treeitem");
      folder.setAttribute("aria-expanded", String(opened));
      folder.append(element("span", "tree-caret", opened ? "\u25be" : "\u25b8"), element("span", "tree-icon folder"), element("span", "", vendor));
      folder.addEventListener("click", () => {
        if (openVendors.has(vendor)) openVendors.delete(vendor); else openVendors.add(vendor);
        renderTree();
      });
      tree.appendChild(folder);
      if (opened) for (const model of models) item(model, 1);
    }
  }

  function renderDeviceForm() {
    const d = editing;
    const surface = d.kind === "surface";
    $("dev-model").textContent = d.model;
    if (document.activeElement !== $("dev-manufacturer")) $("dev-manufacturer").value = d.manufacturer || "";
    if (document.activeElement !== $("dev-name")) $("dev-name").value = d.name || "";
    const generic = GENERIC.find((g) => g.model === d.model);
    const about = surface ? ABOUT[d.protocol] || ABOUT.mackie : ABOUT[d.mpe ? "mpe" : d.kind] || ABOUT.keyboard;
    $("dev-about").textContent = generic ? `${generic.about}${surface ? " " + about : ""}` : about;

    // A control surface: its protocol and both ports, none of a keyboard's notes, channels, filters or sync.
    for (const row of document.querySelectorAll("#dev-dialog .dev-notes")) row.hidden = surface;
    for (const row of document.querySelectorAll("#dev-dialog .dev-surface")) row.hidden = !surface;
    $("dev-protocol").value = d.protocol || "mackie";
    const note = !surface ? ""
      : !d.input && !d.output ? "Choose both ports: Receive From is the one the surface sends from, Send To the one it listens to."
      : !d.input ? "Choose Receive From: the port the surface sends from, so it plays Sonify."
      : !d.output ? "Choose Send To too, so the surface's motor faders, lights, names and meters follow Sonify."
      : "";
    $("dev-surface-note").textContent = note;
    $("dev-surface-note").hidden = !note;

    // Channels: All, and each of the sixteen.
    const buttons = $("dev-channel-buttons");
    if (!buttons.children.length) {
      for (let c = 1; c <= 16; c++) {
        const button = element("button", "", String(c));
        button.type = "button";
        button.dataset.channel = String(c);
        button.addEventListener("click", () => {
          const bit = 1 << (c - 1);
          const next = editing.channels ^ bit;
          editing.channels = next === 0 ? editing.channels : next;   // at least one
          renderDeviceForm();
        });
        buttons.appendChild(button);
      }
    }
    for (const button of buttons.children) button.setAttribute("aria-pressed", String(Boolean(d.channels & (1 << (Number(button.dataset.channel) - 1)))));
    $("dev-all").setAttribute("aria-pressed", String(d.channels === 0xffff));

    const portOptions = (select, ports, chosen) => {
      const list = [["", "None"]].concat(ports.map((p) => [p, p]));
      if (chosen && !ports.includes(chosen)) list.push([chosen, `${chosen} (not connected)`]);
      options(select, list, chosen || "");
    };
    portOptions($("dev-input"), inputs(), d.input);
    portOptions($("dev-output"), outputs(), d.output);

    $("dev-f-aftertouch").checked = d.filterAftertouch;
    $("dev-f-program").checked = d.filterProgramChange;
    $("dev-f-pitchbend").checked = d.filterPitchBend;
    $("dev-f-controllers").checked = d.filterControllers;
    $("dev-split").checked = d.splitChannels;
    $("dev-default").checked = d.defaultInput;
    $("dev-mpe").checked = d.mpe;
    if (document.activeElement !== $("dev-range")) $("dev-range").value = String(d.pitchRange);
    $("dev-range").disabled = !d.mpe;
    $("dev-clock").checked = d.sendClock;
    $("dev-clock-start").checked = d.clockStart;
    $("dev-clock-start").disabled = !d.sendClock;
    $("dev-timecode").checked = d.sendTimecode;
    const noOutput = !d.output;
    for (const id of ["dev-clock", "dev-timecode"]) $(id).disabled = noOutput;
    if (noOutput) $("dev-clock-start").disabled = true;
    $("dev-sync-note").hidden = surface || !noOutput;
  }

  function saveDevice() {
    const d = editing;
    d.name = $("dev-name").value.trim() || nameOf(pickedModel) || d.model;
    d.manufacturer = $("dev-manufacturer").value.trim();
    d.pitchRange = Hub.clamp(Math.round(Number($("dev-range").value) || 48), 1, 96);
    if (!d.output) { d.sendClock = false; d.clockStart = false; d.sendTimecode = false; }
    if (d.kind === "surface") {
      d.protocol = $("dev-protocol").value || "mackie";
      Object.assign(d, SURFACE_OFF);
    }
    Hub.send("midiDeviceSave", { device: d });
    $("dev-dialog").hidden = true;
    editing = null;
  }

  // ---------------------------------------------------------------- events
  Hub.commands.settings = () => open();
  Hub.commands.audioSetup = () => open("audio");
  Hub.commands.pluginFolders = () => open("locations", "plugins");
  Hub.settings = { open, close, state: () => settings, audio: () => audio, catalogue, inputRank };

  Hub.on("settings", (data) => {
    settings = data;
    if (data.appearance) Hub.setAppearance(data.appearance);
    if (data.keymap && data.keymap !== Hub.keys.current()) Hub.keys.use(data.keymap);
    render();
    if (editing && !$("dev-dialog").hidden) renderDeviceForm();
  });

  Hub.on("audioSetup", (data) => {
    audio = data;
    if (tab === "audio") render();
  });

  // Where Main plays (Song Setup) shows on the Audio Setup tab.
  Hub.on("tracks", () => {
    if (tab === "audio") render();
  });

  Hub.on("library", (data) => {
    library = data;
    if (tab === "locations") render();
  });

  Hub.on("ready", () => {
    for (const button of document.querySelectorAll("[data-settings-tab]"))
      button.addEventListener("click", () => showTab(button.dataset.settingsTab));
    for (const button of document.querySelectorAll("[data-locations-tab]"))
      button.addEventListener("click", () => { locationsTab = button.dataset.locationsTab; renderLocations(); });

    $("settings-close").addEventListener("click", close);
    $("settings-x").addEventListener("click", close);

    // Escape closes the device dialog, then Settings, wherever the keyboard is (and nothing behind them).
    window.addEventListener("keydown", (event) => {
      if (event.key !== "Escape" || $("settings").hidden) return;
      if (document.querySelector(".popup-menu")) return;   // an open list takes its own Escape first
      event.preventDefault();
      event.stopImmediatePropagation();
      if (!$("dev-dialog").hidden) { $("dev-dialog").hidden = true; editing = null; }
      else close();
    }, true);

    // Appearance: seen while dragging, kept when let go.
    for (const [id, key] of [["set-luminance", "luminance"], ["set-contrast", "contrast"]]) {
      const input = $(id);
      input.addEventListener("input", () => {
        Hub.setAppearance({ [key]: Number(input.value) });
        $(`${id}-value`).textContent = input.value;
        fill(input);
      });
      input.addEventListener("change", () => Hub.send("setPreference", { [key]: Number(input.value) }));
    }
    $("set-appearance-reset").addEventListener("click", () => {
      Hub.setAppearance({ luminance: 0, contrast: 50 });
      Hub.send("setPreference", { luminance: 0, contrast: 50 });
      renderAppearance();
    });

    $("set-keymap").addEventListener("change", (event) => {
      Hub.keys.use(event.target.value);
      Hub.send("setPreference", { keymap: event.target.value });
      renderKeys();
    });

    $("set-discoverable").addEventListener("change", (event) => Hub.send("setPreference", { discoverable: event.target.checked }));

    $("set-userdata-browse").addEventListener("click", () => Hub.send("chooseUserDataFolder"));
    $("set-userdata-default").addEventListener("click", () => Hub.send("resetUserDataFolder"));
    $("set-autosave").addEventListener("change", (event) => Hub.send("setPreference", { autosave: event.target.checked }));
    $("set-autosave-minutes").addEventListener("change", (event) => {
      const minutes = Hub.clamp(Math.round(Number(event.target.value) || 5), 1, 120);
      event.target.value = String(minutes);
      Hub.send("setPreference", { autosaveMinutes: minutes });
    });
    $("set-folders-add").addEventListener("click", () => Hub.send("addLibraryFolder"));
    $("set-folders-defaults").addEventListener("click", () => Hub.send("addDefaultLibraryFolders"));
    $("set-folders-rescan").addEventListener("click", () => Hub.run("rescanLibrary"));

    // Audio: each change goes to the device at once, and the page hears how it went.
    $("set-audio-type").addEventListener("change", (event) => Hub.send("audioSetupSet", { type: event.target.value }));
    $("set-audio-device").addEventListener("change", (event) => Hub.send("audioSetupSet", { device: event.target.value }));
    $("set-audio-output").addEventListener("change", (event) => Hub.send("audioSetupSet", { output: event.target.value }));
    $("set-audio-input").addEventListener("change", (event) => Hub.send("audioSetupSet", { input: event.target.value }));
    $("set-audio-rate").addEventListener("change", (event) => Hub.send("audioSetupSet", { rate: Number(event.target.value) }));
    $("set-audio-buffer").addEventListener("change", (event) => Hub.send("audioSetupSet", { buffer: Number(event.target.value) }));
    $("set-audio-io").addEventListener("click", () => {
      close();
      Hub.songSetup.open("io", "outputs");
    });
    $("set-audio-panel").addEventListener("click", () => Hub.send("audioControlPanel"));
    $("set-audio-test").addEventListener("click", () => Hub.send("audioTestTone"));
    $("set-release").addEventListener("change", (event) => Hub.send("setPreference", { releaseInBackground: event.target.checked }));

    // MIDI Setup: the external devices.
    $("set-device-add").addEventListener("click", () => editDevice(null));
    $("set-device-edit").addEventListener("click", () => {
      const device = ((settings && settings.devices) || []).find((d) => d.id === chosenDevice);
      if (device) editDevice(device);
    });
    $("set-device-remove").addEventListener("click", () => {
      if (chosenDevice) Hub.send("midiDeviceRemove", { id: chosenDevice });
      chosenDevice = "";
    });
    $("set-timecode-rate").addEventListener("change", (event) => Hub.send("setPreference", { timecodeRate: event.target.value }));

    $("dev-search").addEventListener("input", renderTree);
    $("dev-all").addEventListener("click", () => {
      editing.channels = editing.channels === 0xffff ? 1 : 0xffff;
      renderDeviceForm();
    });
    $("dev-manufacturer").addEventListener("input", (event) => { editing.manufacturer = event.target.value; });
    $("dev-name").addEventListener("input", (event) => { editing.name = event.target.value; });
    $("dev-input").addEventListener("change", (event) => { editing.input = event.target.value; autoInput = false; renderDeviceForm(); });
    $("dev-output").addEventListener("change", (event) => { editing.output = event.target.value; autoOutput = false; renderDeviceForm(); });
    $("dev-protocol").addEventListener("change", (event) => { editing.protocol = event.target.value; renderDeviceForm(); });
    for (const [id, key] of [["dev-f-aftertouch", "filterAftertouch"], ["dev-f-program", "filterProgramChange"],
                             ["dev-f-pitchbend", "filterPitchBend"], ["dev-f-controllers", "filterControllers"],
                             ["dev-split", "splitChannels"], ["dev-default", "defaultInput"], ["dev-mpe", "mpe"],
                             ["dev-clock", "sendClock"], ["dev-clock-start", "clockStart"], ["dev-timecode", "sendTimecode"]]) {
      $(id).addEventListener("change", (event) => { editing[key] = event.target.checked; renderDeviceForm(); });
    }
    $("dev-range").addEventListener("change", (event) => {
      editing.pitchRange = Hub.clamp(Math.round(Number(event.target.value) || 48), 1, 96);
      event.target.value = String(editing.pitchRange);
    });
    $("dev-ok").addEventListener("click", saveDevice);
    $("dev-cancel").addEventListener("click", () => { $("dev-dialog").hidden = true; editing = null; });
  });
})();
