// D:\workspace\Fanan_web_hub\src\hub-content.js
//
// The Browser's Fanan tab: what Fanan publishes (loops, samples, songs, WCLAP plugins),
// for everyone, no account needed. ▶ previews an item; Install downloads it into
// Documents\Fanan Web Hub\Fanan Content: loops and samples then show in the Files tab,
// songs open, plugins appear in the Browser. Update appears when Fanan has a newer one.

(() => {
  "use strict";

  const { $, element } = Hub;
  const KINDS = [["all", "All"], ["loop", "Loops"], ["sample", "Samples"], ["song", "Songs"], ["plugin", "Plugins"]];
  let items = [], error = "", configured = true, loaded = false, kind = "all";
  const previews = new Map();    // id -> preview file
  const progress = new Map();    // id -> fraction while installing

  function sizeText(bytes) {
    const mb = (bytes || 0) / (1024 * 1024);
    return mb >= 1 ? `${mb.toFixed(1)} MB` : `${Math.max(1, Math.round((bytes || 0) / 1024))} KB`;
  }

  function metaOf(item) {
    const parts = [{ loop: "Loop", sample: "Sample", song: "Song", plugin: "Plugin" }[item.kind]];
    if (item.bpm) parts.push(`${item.bpm} BPM`);
    if (item.key) parts.push(item.key);
    parts.push(sizeText(item.size));
    return parts.join(" · ");
  }

  function actionOf(item) {
    if (progress.has(item.id)) return null;
    if (item.update) return ["Update", "Fanan has a newer version"];
    if (!item.installed) return ["Install", item.kind === "plugin" ? "Install it into the Browser" : "Download it to this computer"];
    if (item.kind === "song") return ["Open", "Open the song"];
    if (item.kind === "plugin") return null;
    return ["Show", "Show it in the Files tab"];
  }

  function render() {
    const list = $("fanan-list");
    list.replaceChildren();
    for (const chip of $("fanan-kinds").children) chip.setAttribute("aria-pressed", String(chip.dataset.kind === kind));

    if (!configured || error) {
      list.appendChild(element("p", "browser-note", error || "Fanan's content isn't set up in this build yet."));
      return;
    }
    if (!loaded) {
      list.appendChild(element("p", "browser-note", "Loading Fanan's content…"));
      return;
    }

    const text = $("browser-search").value.trim().toLowerCase();
    // MIDI tools have the Browser's MIDI tab (hub-miditools.js).
    const shown = items.filter((item) => item.kind !== "miditool" && (kind === "all" || item.kind === kind)
      && (!text || [item.title, item.description, ...(item.tags || [])].join(" ").toLowerCase().includes(text)));
    if (!shown.length) list.appendChild(element("p", "browser-note", items.length ? "Nothing found." : "Nothing published yet."));

    const previewing = Hub.state.status && Hub.state.status.previewing;
    for (const item of shown) {
      const row = element("div", "fanan-item");
      row.dataset.id = item.id;
      const text = element("div", "fanan-text");
      const title = element("div", "fanan-title", item.title);
      if (item.installed && !item.update) title.appendChild(element("span", "fanan-badge", "Installed"));
      text.append(title, element("div", "muted small", metaOf(item)));
      if (item.description) text.appendChild(element("div", "fanan-description small", item.description));
      if (progress.has(item.id)) {
        const bar = element("div", "export-bar fanan-bar");
        const fill = element("i");
        fill.style.width = `${Math.round(progress.get(item.id) * 100)}%`;
        bar.appendChild(fill);
        text.appendChild(bar);
      }
      row.appendChild(text);

      if (item.kind !== "plugin") {
        const playing = previews.has(item.id) && previewing === previews.get(item.id);
        const play = element("button", "file-play fanan-play", playing ? "■" : "▶");
        play.type = "button";
        play.title = playing ? "Stop the preview" : "Preview";
        play.setAttribute("aria-label", `${playing ? "Stop" : "Preview"} ${item.title}`);
        play.addEventListener("click", () => {
          if (playing) Hub.send("stopPreview");
          else Hub.send("contentPreview", { id: item.id });
        });
        row.appendChild(play);
      }

      const action = actionOf(item);
      if (action) {
        const button = element("button", "plain-button small" + (action[0] === "Install" || action[0] === "Update" ? " primary" : ""), action[0]);
        button.type = "button";
        button.title = action[1];
        button.addEventListener("click", () => {
          if (action[0] === "Install" || action[0] === "Update") {
            progress.set(item.id, 0);
            render();
            Hub.send("contentInstall", { id: item.id });
          } else {
            if (action[0] === "Show" && Hub.showBrowserTab) Hub.showBrowserTab("files");
            Hub.send("contentOpen", { id: item.id });
          }
        });
        row.appendChild(button);
      }
      $("fanan-list").appendChild(row);
    }
  }

  Hub.on("contentTab", () => {
    if (!loaded && !error) Hub.send("contentList");
    render();
  });

  Hub.on("fananContent", (data) => {
    items = data.items || [];
    error = data.error || "";
    configured = data.configured !== false;
    loaded = true;
    render();
  });

  Hub.on("contentProgress", (p) => {
    progress.set(p.id, Hub.clamp01(p.fraction || 0));
    const fill = document.querySelector(`.fanan-item[data-id="${p.id}"] .fanan-bar i`);
    if (fill) fill.style.width = `${Math.round(progress.get(p.id) * 100)}%`;
    else render();
  });

  Hub.on("contentResult", (r) => {
    progress.delete(r.id);
    if (!r.ok) Hub.toast(r.error || "That didn't work.");
    else if (r.kind === "plugin") Hub.toast(`Installed “${r.title}”: it's in the Browser's Instruments and Effects`);
    else if (r.kind === "song") Hub.toast(`Installed “${r.title}”: click Open to play it`);
    else Hub.toast(`Installed “${r.title}”: it's in Files, under Fanan Content`);
    render();
  });

  Hub.on("contentPreviewing", (p) => {
    previews.set(p.id, p.file);
    render();
  });

  // The preview buttons follow what's playing.
  let previewing = "";
  Hub.on("status", (s) => {
    if ((s.previewing || "") === previewing) return;
    previewing = s.previewing || "";
    if (loaded && $("browser").classList.contains("fanan")) render();
  });

  Hub.fananContent = { items: () => items };

  Hub.on("ready", () => {
    const chips = $("fanan-kinds");
    for (const [value, label] of KINDS) {
      const chip = element("button", "fanan-kind", label);
      chip.type = "button";
      chip.dataset.kind = value;
      chip.addEventListener("click", () => { kind = value; render(); });
      chips.appendChild(chip);
    }
    $("fanan-refresh").addEventListener("click", () => { loaded = false; error = ""; render(); Hub.send("contentList"); });
    $("browser-search").addEventListener("input", () => { if ($("browser").classList.contains("fanan")) render(); });
  });
})();
