// D:\workspace\Fanan_web_hub\src\hub-devices.js
//
// Sonify devices: MIDI devices made by asking Sonify AI in the Plugins Creator Studio's Build with AI
// (type MIDI FX), the way Max for Live devices are built. A device sits in an instrument track's MIDI
// FX, hears the notes, plays its own, and can move Sonify's controls. Every device made is kept in My
// Devices (Documents\Sonify\Plugins, one .sip file each, kind "midi"), which the MIDI FX menus list.
//
// This page keeps what the rest of it needs to know about them (My Devices, whether devices can be
// built here) and sends the Plugins Creator Studio a device to work on: the one in a track's MIDI FX
// slot (its window's "Open in Plugins Creator Studio"), or a new one. The studio shows it in Build
// with AI, with its own conversation.

(() => {
  "use strict";

  let studio = { available: true, why: "", open: false, library: [] };

  /** The device a song's place came from, when My Devices has a newer version of it. */
  function newerFor(item) {
    if (!item || !item.device) return null;
    const entry = (studio.library || []).find((e) => e.id === item.device.deviceId);
    return entry && entry.built && entry.version > (item.device.version || 0) ? entry : null;
  }

  /** The Plugins Creator Studio in front, on Build with AI, where the device is made. */
  function showCreator() {
    if (Hub.studios) Hub.studios.activate("plugins");
    if (Hub.creator) Hub.creator.showTab(0);
  }

  // ---------------------------------------------------------------- from the engine
  Hub.on("deviceStudio", (data) => {
    studio = { ...data };
  });

  Hub.on("deviceStudioMessage", (data) => {
    const text = data.message || data.error;
    if (text) Hub.toast(text.split("\n")[0]);
  });

  // ---------------------------------------------------------------- for the rest of the page
  Hub.devices = {
    library: () => (studio.library || []).filter((entry) => entry.built),
    available: () => studio.available,
    why: () => studio.why,
    newerFor,
    /** The device in a track's MIDI FX slot, in the Plugins Creator Studio's Build with AI. */
    openSlot(trackId, fxId) {
      Hub.send("deviceStudio", { action: "openSlot", trackId, fxId });
      showCreator();
    },
    /** A new device (fresh) or the one open, in the Plugins Creator Studio's Build with AI. */
    openStudio(fresh = false) {
      if (fresh) Hub.send("deviceStudio", { action: "new" });
      showCreator();
    }
  };

  Hub.on("ready", () => {
    Hub.send("deviceStudio", { action: "state" });
  });
})();
