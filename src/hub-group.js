// D:\workspace\Fanan_web_hub\src\hub-group.js
//
// Several tracks and channels at once, in the track list and the Console alike:
//   Click            selects the one track or channel
//   Ctrl + click     adds it to the selection, or takes it out
//   Shift + click    adds every one from the last clicked to it, keeping the rest
// With several selected, the selection works as one:
//   Mute, Solo, Record and Monitor     a click on one switches them all the same way
//   Faders and pans                    move together, each by as much (hub-core.js)
//   Right-click                        Duplicate, Remove, Color, Rename (numbered: "Vocals 1",
//                                      "Vocals 2"...), Select All, Add Bus for Selected Channels;
//                                      on a bus, Select What Plays Into It; then the track menu
//                                      (hub-tracks.js: Add Tracks, Collapse, Track Presets...)
//   Dragging a header (its number or name), or a strip (its name)   moves them all there
// Track > Duplicate Selected Tracks and Remove Selected Tracks do the same. Removing, duplicating,
// moving and renaming are each one undo step; mixing isn't, as in Studio One.

(() => {
  "use strict";

  const { $, element } = Hub;
  const COLOURS = ["#e5484d", "#f08c2e", "#e9c93b", "#4cbf67", "#3cc0c8", "#3e82f7", "#9b6cf2", "#e85aa8",
                   "#a8363a", "#b8681f", "#a48b22", "#2f8a48", "#26878d", "#2b5cb3", "#6c47b0", "#a33d77"];
  const ACTIONS = ["mute", "solo", "recordArm", "monitor", "polarity"];
  let reorder = null;
  let popover = null;

  const many = (ids, one, more) => (ids.length > 1 ? more.replace("#", String(ids.length)) : one);

  /** A click on a header or strip: the selection, as the modifier keys say. */
  function click(event, id) {
    Hub.selectTrack(id, event.ctrlKey || event.metaKey ? "toggle" : event.shiftKey ? "range" : "only");
  }

  /** Mute, Solo, Record or Monitor clicked: the selection switches with it, all the same way. */
  function toggle(id, action, button) {
    const track = Hub.trackById(id);
    const on = button.getAttribute("aria-pressed") !== "true";
    // FX channels and buses have no recording and no monitoring.
    const ids = Hub.groupOf(id).filter((other) => {
      const channel = Hub.trackById(other);
      return channel && !((channel.kind === "fx" || channel.kind === "bus") && (action === "recordArm" || action === "monitor"));
    });

    button.setAttribute("aria-pressed", String(on));
    if (ids.length < 2 || !ACTIONS.includes(action)) {
      if (track) track[action] = on;
      Hub.send("trackSettings", { id, [action]: on });
      return;
    }

    // Shown at once in the track list and the Console; the engine confirms with a tracks update.
    for (const other of ids) {
      const channel = Hub.trackById(other);
      if (channel) channel[action] = on;
      document.querySelectorAll(`.track[data-id="${other}"] [data-action="${action}"], .strip[data-id="${other}"] [data-action="${action}"]`)
        .forEach((each) => each.setAttribute("aria-pressed", String(on)));
    }
    Hub.send("tracksSettings", { ids, [action]: on });
  }

  // ---------------------------------------------------------------- the menu
  function menu(id, x, y) {
    if (!Hub.isTrackSelected(id)) Hub.selectTrack(id);
    const ids = Hub.groupOf(id);
    const at = { getBoundingClientRect: () => ({ left: x, right: x, top: y, bottom: y }) };

    const channel = Hub.trackById(id);
    const feeders = channel && channel.kind === "bus" ? feedersOf(id) : [];
    const tracks = ids.filter(isTrack);
    const items = [
      { label: many(ids, "Duplicate Track", "Duplicate # Tracks"), value: "duplicate" },
      { label: many(ids, "Remove Track", "Remove # Tracks"), value: "remove" },
      { label: "Color…", value: "colour" },
      { label: many(ids, "Rename…", "Rename # Tracks…"), value: "rename" },
      { label: "Select All Tracks", value: "all" },
      { label: many(ids, "Invert Polarity", "Invert Polarity of # Channels"), value: "polarity", checked: Boolean(channel && channel.polarity),
        title: "Turn the sound upside down (\u00d8), as when two microphones fight each other" },
      { label: tracks.length > 1 ? `Add Bus for ${tracks.length} Selected Channels` : "Add Bus for Selected Channels", value: "bus", disabled: !tracks.length }
    ];
    if (channel && channel.kind === "bus")
      items.push({ label: `Select What Plays Into It (${feeders.length})`, value: "feeders", disabled: !feeders.length });
    if (channel && channel.kind !== "audio" && channel.kind !== "fx" && channel.kind !== "bus" && Hub.score)
      items.push({ label: "Show as Score\u2026", value: "score" });
    // Follow Chords: the chord track's chords move the track's notes as they play (its MIDI FX,
    // first in the chain; the notes in the clips stay as they are).
    const follower = channel && channel.kind === "instrument" ? (channel.midiFx || []).find((d) => d.type === "followChords") : null;
    if (channel && channel.kind === "instrument")
      items.push({ label: "Follow Chords", value: "followChords", checked: Boolean(follower),
                   title: "The chord track's chords (Master Song Channel > Chords) move this track's notes as they play; the notes in its clips stay as they are" });
    if (channel && channel.kind === "audio" && Hub.importMedia)
      items.push({ label: "Load Media\u2026", value: "loadMedia", title: "Audio files onto this track at the song position (more than one: the others on new tracks under it)" });

    // Then Studio One's track menu (hub-tracks.js): adding tracks, collapsing, track presets.
    const trackItems = Hub.trackMenu ? Hub.trackMenu.items() : [];
    if (trackItems.length) items.push({ separator: true }, ...trackItems.map((item) => (item.separator ? item : { ...item, value: { track: item.value } })));

    Hub.popupMenu(at, items, (value) => {
      if (value && typeof value === "object" && value.track) { Hub.trackMenu.run(value.track); return; }
      if (value === "duplicate") Hub.send("duplicateTracks", { ids });
      if (value === "remove") Hub.send("removeTracks", { ids });
      if (value === "colour") colours(ids, at);
      if (value === "rename") rename(ids, at);
      if (value === "all") selectAll();
      if (value === "bus") Hub.send("addBusChannel", { ids: tracks });
      if (value === "feeders") selectFeeders(id);
      if (value === "score") Hub.score.open(id);
      if (value === "followChords") {
        if (follower) Hub.send("midiFxRemove", { trackId: id, fxId: follower.id });
        else if (Hub.midiFx) Hub.midiFx.addDevice(id, { type: "followChords" }, 0, false);
        if (!follower && !Hub.tempoMap.chords.length) Hub.toast("Follow Chords is on. Put chords on the chord track (Master Song Channel > Chords) for it to follow.");
      }
      if (value === "loadMedia") Hub.importMedia.choose(id);
      if (value === "polarity") {
        const on = !(channel && channel.polarity);
        for (const other of ids) { const each = Hub.trackById(other); if (each) each.polarity = on; }
        document.querySelectorAll(ids.map((other) => `[data-id="${other}"] [data-action="polarity"]`).join(", "))
          .forEach((each) => each.setAttribute("aria-pressed", String(on)));
        Hub.send("tracksSettings", { ids, polarity: on });
      }
    });
  }

  /** Tracks (not FX channels nor buses): what can play into a bus. */
  const isTrack = (id) => { const channel = Hub.trackById(id); return Boolean(channel) && channel.kind !== "fx" && channel.kind !== "bus"; };
  const feedersOf = (busId) => Hub.state.tracks.filter((track) => track.bus === busId).map((track) => track.id);

  /** What plays into a bus becomes the selection (shown in the track list and the Console). */
  function selectFeeders(busId) {
    const feeders = feedersOf(busId);
    const bus = Hub.trackById(busId);
    if (!feeders.length) return;
    Hub.selectTrack(feeders[0]);
    for (const id of feeders.slice(1)) Hub.selectTrack(id, "toggle");
    Hub.toast(`${bus ? bus.name : "The bus"} plays ${feeders.map((id) => (Hub.trackById(id) || {}).name).join(", ")}`);
  }

  function selectAll() {
    const order = Hub.channelOrder();
    if (!order.length) return;
    Hub.selectTrack(order[0]);
    Hub.selectTrack(order[order.length - 1], "range");
  }

  function openPopover(content, at) {
    closePopover();
    const box = element("div", "group-popover");
    box.appendChild(content);
    document.body.appendChild(box);
    const r = at.getBoundingClientRect();
    box.style.left = Math.max(4, Math.min(window.innerWidth - box.offsetWidth - 4, r.left)) + "px";
    box.style.top = Math.max(4, Math.min(window.innerHeight - box.offsetHeight - 4, r.bottom + 2)) + "px";

    const outside = (event) => { if (!box.contains(event.target)) closePopover(); };
    const escape = (event) => { if (event.key === "Escape") { event.stopPropagation(); closePopover(); } };
    document.addEventListener("pointerdown", outside, true);
    window.addEventListener("keydown", escape, true);
    popover = { box, outside, escape };
    return box;
  }

  function closePopover() {
    if (!popover) return;
    document.removeEventListener("pointerdown", popover.outside, true);
    window.removeEventListener("keydown", popover.escape, true);
    if (popover.box.contains(document.activeElement)) document.activeElement.blur();
    popover.box.remove();
    popover = null;
  }

  /** A palette: the colour goes to every selected channel. */
  function colours(ids, at) {
    const grid = element("div", "group-colours");
    grid.setAttribute("role", "group");
    grid.setAttribute("aria-label", "Color");
    for (const colour of COLOURS) {
      const swatch = element("button", "group-swatch");
      swatch.type = "button";
      swatch.style.background = colour;
      swatch.title = colour;
      swatch.setAttribute("aria-label", colour);
      swatch.addEventListener("click", () => {
        for (const id of ids) { const channel = Hub.trackById(id); if (channel) channel.colour = colour; }
        Hub.send("tracksSettings", { ids, colour });
        closePopover();
      });
      grid.appendChild(swatch);
    }
    openPopover(grid, at);
  }

  /** One name: "Vocals" for one track, "Vocals 1", "Vocals 2"... for several, in their order. */
  function rename(ids, at) {
    const first = Hub.trackById(ids[0]);
    const form = element("form", "group-rename");
    const label = element("label", "", many(ids, "Name", "Name the # tracks"));
    const input = element("input");
    input.type = "text";
    input.value = first ? first.name.replace(/\s+\d+$/, "") : "";
    input.spellcheck = false;
    const ok = element("button", "plain-button small primary", "Rename");
    ok.type = "submit";
    label.appendChild(input);
    form.append(label, ok);
    form.addEventListener("submit", (event) => {
      event.preventDefault();
      const name = input.value.trim();
      if (!name) return;
      const names = ids.map((id, index) => ({ id, name: ids.length > 1 ? `${name} ${index + 1}` : name }));
      Hub.send("renameTracks", { names });
      closePopover();
    });
    input.addEventListener("keydown", (event) => event.stopPropagation());
    openPopover(form, at);
    input.focus();
    input.select();
  }

  // ---------------------------------------------------------------- moving by dragging
  /**
      A press on a header's number or name, or a strip's name: dragging past a few pixels moves
      the selection (or the one channel, when it isn't selected) to where it's dropped, among the
      channels of its kind. axis "y" for the track list, "x" for the Console. items() gives the
      rows or strips in order: [{id, element}].
  */
  function startReorder(event, id, axis, items) {
    if (event.button !== 0) return;
    reorder = { id, axis, items, x: event.clientX, y: event.clientY, pointer: event.pointerId, moving: null, line: null, before: null };
    window.addEventListener("pointermove", moveReorder, true);
    window.addEventListener("pointerup", endReorder, true);
    window.addEventListener("pointercancel", endReorder, true);
  }

  function moveReorder(event) {
    if (!reorder || event.pointerId !== reorder.pointer) return;
    const along = reorder.axis === "y" ? event.clientY - reorder.y : event.clientX - reorder.x;

    if (!reorder.moving) {
      if (Math.abs(along) < 5) return;
      const ids = Hub.isTrackSelected(reorder.id) ? Hub.groupOf(reorder.id) : [reorder.id];
      const sortOf = (id) => { const kind = (Hub.trackById(id) || {}).kind; return kind === "fx" || kind === "bus" ? kind : "track"; };
      const kinds = new Set(ids.map(sortOf));
      reorder.moving = ids;
      // Among channels of the same kind: tracks among tracks, buses among buses, FX channels among FX channels.
      reorder.pool = kinds.size === 1 ? reorder.items().filter((item) => kinds.has(sortOf(item.id))) : reorder.items();
      reorder.line = element("div", "reorder-line " + reorder.axis);
      document.body.appendChild(reorder.line);
      document.body.classList.add("reordering");
      for (const item of reorder.pool) if (ids.includes(item.id)) item.element.classList.add("reorder-moving");
    }

    event.preventDefault();
    const at = reorder.axis === "y" ? event.clientY : event.clientX;
    let before = 0, edge = null;

    for (const item of reorder.pool) {
      const r = item.element.getBoundingClientRect();
      const middle = reorder.axis === "y" ? r.top + r.height / 2 : r.left + r.width / 2;
      if (at < middle) { before = item.id; edge = r; break; }
    }

    const last = reorder.pool.length ? reorder.pool[reorder.pool.length - 1].element.getBoundingClientRect() : null;
    const box = edge || last;
    if (!box) return;
    reorder.before = before;
    const line = reorder.line.style;
    if (reorder.axis === "y") {
      line.left = box.left + "px"; line.width = box.width + "px";
      line.top = (edge ? box.top : box.bottom) - 1 + "px";
    } else {
      line.top = box.top + "px"; line.height = box.height + "px";
      line.left = (edge ? box.left : box.right) - 1 + "px";
    }
  }

  function endReorder(event) {
    if (!reorder || event.pointerId !== reorder.pointer) return;
    const done = reorder;
    reorder = null;
    window.removeEventListener("pointermove", moveReorder, true);
    window.removeEventListener("pointerup", endReorder, true);
    window.removeEventListener("pointercancel", endReorder, true);
    if (!done.moving) return;

    done.line.remove();
    document.body.classList.remove("reordering");
    document.querySelectorAll(".reorder-moving").forEach((el) => el.classList.remove("reorder-moving"));

    // The click that ends a drag selects nothing.
    const swallow = (e) => { e.stopPropagation(); e.preventDefault(); };
    window.addEventListener("click", swallow, { capture: true, once: true });
    setTimeout(() => window.removeEventListener("click", swallow, true), 0);

    // Before the channel it was dropped in front of, or (0) after the last of its kind.
    if (event.type !== "pointerup" || done.before === null || done.moving.includes(done.before)) return;
    Hub.send("moveTracks", { ids: done.moving, before: done.before });
  }

  // ---------------------------------------------------------------- commands
  Hub.commands.removeTrack = () => {
    const ids = Hub.state.selectedTracks.length ? Hub.state.selectedTracks.slice() : Hub.state.selected ? [Hub.state.selected] : [];
    if (ids.length) Hub.send("removeTracks", { ids });
  };

  Hub.commands.duplicateTracks = () => {
    const ids = Hub.state.selectedTracks.length ? Hub.state.selectedTracks.slice() : Hub.state.selected ? [Hub.state.selected] : [];
    if (ids.length) Hub.send("duplicateTracks", { ids });
  };

  Hub.commands.selectAllTracks = selectAll;

  // Track > Add Bus Channel, and Add Bus for Selected Channels (the selected tracks play into it).
  Hub.commands.addBusChannel = () => Hub.send("addBusChannel", {});
  Hub.commands.addBusForSelected = () => {
    const ids = Hub.state.selectedTracks.filter(isTrack);
    if (ids.length) Hub.send("addBusChannel", { ids });
    else Hub.toast("Select the tracks to play into the bus first.");
  };

  Hub.group = { click, toggle, menu, startReorder, closePopover, selectFeeders };
})();
