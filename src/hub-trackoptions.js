// D:\workspace\Fanan_web_hub\src\hub-trackoptions.js
//
// The track list's Options menu: the wrench at the top of the track list, as Studio One's.
// Each option is on or off for Sonify as a whole (kept in Settings, Preferences.h); the menu stays
// open while they're switched.
//
//   Selection   Solo follows Selection: the selected tracks are the soloed ones (as the
//               selection changes; a track can still be soloed by hand meanwhile)
//               Instrument Input follows Selection, Audio Input follows Selection: the track
//               selected monitors, the others of its kind stop (an armed one goes on); the engine
//               does it (MainTrackOptions.cpp)
//   Editing     Locate when clicked in empty space; Cursor follows Edit Position (a clip clicked
//               or moved takes the song position to its start); Automation follows events and No
//               overlap when editing events (the engine, with the clip edit)
//   Visibility  Colorize track controls; Show channel numbers in tracks; Show event names
//   Audio       Monitoring follows record (the engine)
//   Autoscroll  Autoscroll itself (the toolbar's, kept with the song), and Scrolling pauses
//               Autoscroll: scrolled away by hand while playing, the view stays where it was put
//               until the playhead is in it again or the song starts again (hub-arrange.js)
//
// Hub.trackOption(name) is an option as chosen (or its default).

(() => {
  "use strict";

  const { $, element } = Hub;

  // As Preferences::getTrackOptions has them: name, default.
  const DEFAULTS = {
    soloFollowsSelection: false, instrumentInputFollowsSelection: false, audioInputFollowsSelection: false,
    locateOnEmptyClick: false, cursorFollowsEdit: false, automationFollowsEvents: false, noOverlap: false,
    colorizeTracks: false, showTrackNumbers: true, showClipNames: true,
    monitorFollowsRecord: false, scrollPausesAutoscroll: true
  };

  const MENU = [
    ["Selection", [
      ["soloFollowsSelection", "Solo follows Selection", "The selected tracks are the soloed ones"],
      ["instrumentInputFollowsSelection", "Instrument Input follows Selection", "The instrument track selected hears your MIDI devices; the others stop (an armed one goes on)"],
      ["audioInputFollowsSelection", "Audio Input follows Selection", "The audio track selected monitors its input; the others stop (an armed one goes on)"]
    ]],
    ["Editing", [
      ["locateOnEmptyClick", "Locate when clicked in empty space", "A click on the lanes where there's no clip moves the song position there"],
      ["cursorFollowsEdit", "Cursor follows Edit Position", "A clip clicked or moved takes the song position to its start"],
      ["automationFollowsEvents", "Automation follows events", "A clip moved takes its track's automation under it along (a copy copies it)"],
      ["noOverlap", "No overlap when editing events", "A clip moved, resized or pasted over others trims them where it lands"]
    ]],
    ["Visibility", [
      ["colorizeTracks", "Colorize track controls", "Each track's header in its track's colour"],
      ["showTrackNumbers", "Show channel numbers in tracks", "The number on each track's header"],
      ["showClipNames", "Show event names", "The name on each clip"]
    ]],
    ["Audio", [
      ["monitorFollowsRecord", "Monitoring follows record", "Arming a track monitors it; disarming stops it"]
    ]],
    ["Autoscroll", [
      ["follow", "Autoscroll", "Follow the playhead while playing (the toolbar's Autoscroll, kept with the song)"],
      ["scrollPausesAutoscroll", "Scrolling pauses Autoscroll", "Scrolled away by hand while playing, the view stays where you put it until the playhead is in it again, or the song starts again"]
    ]]
  ];

  let options = {};
  let closeMenu = null;

  Hub.trackOption = (name) => (typeof options[name] === "boolean" ? options[name] : Boolean(DEFAULTS[name]));

  /** What the page shows for them. */
  function apply() {
    document.body.classList.toggle("colorize-tracks", Hub.trackOption("colorizeTracks"));
    document.body.classList.toggle("hide-track-numbers", !Hub.trackOption("showTrackNumbers"));
    if (Hub.arrange) Hub.arrange.redraw();
  }

  function isOn(name) {
    return name === "follow" ? Boolean(Hub.arrange && Hub.arrange.view.follow) : Hub.trackOption(name);
  }

  function toggle(name) {
    if (name === "follow") {
      Hub.run("toggleFollow");
      return;
    }
    const on = !Hub.trackOption(name);
    options = Object.assign({}, options, { [name]: on });
    apply();
    Hub.send("setPreference", { trackOptions: { [name]: on } });
    Hub.emit("trackOptions", { name, on });
    if (name === "soloFollowsSelection" && on) soloSelection(true);
  }

  // ---------------------------------------------------------------- the menu
  function openMenu(button) {
    if (closeMenu) { closeMenu(); return; }
    document.querySelectorAll(".popup-menu").forEach((menu) => menu.remove());
    const menu = element("div", "popup-menu with-marks track-options-menu");
    menu.setAttribute("role", "menu");
    button.setAttribute("aria-expanded", "true");

    const render = () => {
      menu.replaceChildren();
      MENU.forEach(([group, items], index) => {
        if (index) menu.appendChild(element("div", "popup-separator"));
        menu.appendChild(element("div", "popup-group", group));
        for (const [name, label, title] of items) {
          const on = isOn(name);
          const item = element("button");
          item.type = "button";
          item.setAttribute("role", "menuitemcheckbox");
          item.setAttribute("aria-checked", String(on));
          item.dataset.option = name;
          item.title = title;
          item.append(element("span", "popup-check", on ? "\u2713" : ""), element("span", "popup-label", label));
          item.addEventListener("click", (event) => {
            event.stopPropagation();
            toggle(name);
            render();
          });
          menu.appendChild(item);
        }
      });
    };

    render();
    document.body.appendChild(menu);
    const r = button.getBoundingClientRect();
    const height = menu.offsetHeight || 0;
    menu.style.left = Math.max(4, Math.min(window.innerWidth - (menu.offsetWidth || 260) - 4, r.left)) + "px";
    menu.style.top = (r.bottom + height + 4 > window.innerHeight ? Math.max(4, r.top - height - 2) : r.bottom + 2) + "px";

    const close = () => {
      menu.remove();
      button.setAttribute("aria-expanded", "false");
      document.removeEventListener("pointerdown", outside, true);
      window.removeEventListener("keydown", escape, true);
      window.removeEventListener("blur", close);
      if (closeMenu === close) closeMenu = null;
    };
    const outside = (event) => { if (!menu.contains(event.target) && !button.contains(event.target)) close(); };
    const escape = (event) => { if (event.key === "Escape") { event.stopPropagation(); close(); } };
    closeMenu = close;
    document.addEventListener("pointerdown", outside, true);
    window.addEventListener("keydown", escape, true);
    window.addEventListener("blur", close);
  }

  // ---------------------------------------------------------------- Solo follows Selection
  // As the selection changes, the tracks in it are soloed and the rest aren't (FX channels and
  // buses keep theirs). Only a change of selection does it, so a solo by hand stays meanwhile.
  let soloedFor = "";

  function soloSelection(force) {
    if (!Hub.trackOption("soloFollowsSelection") || Hub.state.inspectMain) return;
    const tracks = Hub.state.tracks.filter((track) => track.kind !== "fx" && track.kind !== "bus");
    const chosen = tracks.filter((track) => Hub.state.selectedTracks.includes(track.id)).map((track) => track.id);
    const key = chosen.join(",");
    if (!chosen.length || (key === soloedFor && !force)) return;
    soloedFor = key;

    const on = [], off = [];
    for (const track of tracks) {
      const wanted = chosen.includes(track.id);
      if (Boolean(track.solo) === wanted) continue;
      track.solo = wanted;   // shown at once; the engine confirms
      (wanted ? on : off).push(track.id);
      document.querySelectorAll(`.track[data-id="${track.id}"] [data-action="solo"], .strip[data-id="${track.id}"] [data-action="solo"]`)
        .forEach((each) => each.setAttribute("aria-pressed", String(wanted)));
    }
    if (on.length) Hub.send("tracksSettings", { ids: on, solo: true });
    if (off.length) Hub.send("tracksSettings", { ids: off, solo: false });
  }

  // ---------------------------------------------------------------- wiring
  Hub.on("settings", (data) => {
    if (data && data.trackOptions && typeof data.trackOptions === "object") options = Object.assign({}, data.trackOptions);
    apply();
  });
  Hub.on("selection", () => soloSelection(false));

  Hub.on("ready", () => {
    const button = $("track-options");
    if (!button) return;
    button.addEventListener("click", (event) => { event.stopPropagation(); openMenu(button); });
    apply();
  });
})();
