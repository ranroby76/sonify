// D:\workspace\Fanan_web_hub\src\hub-scan.js
//
// The startup question, then the scanning page. At every start this page first asks whether
// to load the user's third-party plugins or use only Sonify's internal ones. Yes: plugins are
// read (new and changed ones) with the scanning page in front, and the welcome page appears
// once the scan is finished. No: no scan, straight to the welcome page, and only Sonify's own
// plugins are offered until the next start. Plugins are also read on every rescan, with this
// page in front. Each plugin is read by a copy of the Hub of its own, so one that crashes or
// hangs is reported here and can't take the Hub down.

(() => {
  "use strict";

  const { $, element } = Hub;
  let scan = { state: "starting", startup: true, total: 0, done: 0, problems: [] };
  let finishing = null;
  let chosen = false;   // the question at startup has its answer

  const plural = (n, one, many) => `${n} ${n === 1 ? one : many}`;

  function render() {
    const running = scan.state === "running" || scan.state === "starting";
    const fraction = scan.total > 0 ? scan.done / scan.total : running ? 0 : 1;
    $("scan-title").textContent = running ? (scan.startup ? "Checking your plugins" : "Scanning plugins") : "Plugins checked";
    $("scan-fill").style.width = `${Math.round(fraction * 100)}%`;
    $("scan-bar").setAttribute("aria-valuenow", String(Math.round(fraction * 100)));

    if (scan.state === "starting") {
      $("scan-sub").textContent = "Starting…";
      $("scan-count").textContent = "";
      $("scan-current").textContent = "";
    } else if (running) {
      $("scan-sub").textContent = scan.total === 1 ? "1 new or changed plugin file to read." : `${scan.total} new or changed plugin files to read.`;
      $("scan-count").textContent = `${Math.min(scan.done + 1, scan.total)} of ${scan.total}`;
      $("scan-current").textContent = scan.current ? `Reading ${scan.current}` : "";
    } else {
      const parts = [plural(scan.plugins || 0, "plugin ready", "plugins ready")];
      if (scan.found > 0) parts.push(`${scan.found} new`);
      if (scan.problems.length) parts.push(`${plural(scan.problems.length, "file", "files")} couldn't be read`);
      if (scan.state === "cancelled") parts.push("the rest are read next time");
      $("scan-sub").textContent = parts.join(" · ");
      $("scan-count").textContent = "";
      $("scan-current").textContent = "";
    }

    const list = $("scan-problem-list");
    list.replaceChildren();
    for (const problem of scan.problems) {
      const item = element("li");
      item.append(element("span", "scan-problem-name", problem.name), element("span", "scan-problem-why", problem.error));
      list.appendChild(item);
    }
    $("scan-problems").hidden = scan.problems.length === 0;

    const skip = $("scan-skip");
    skip.hidden = !running || scan.state === "starting";
    skip.textContent = scan.startup ? "Skip" : "Cancel";
    skip.title = scan.startup ? "Go on now: the rest are read at the next start" : "Stop scanning: the rest are read next time";
    $("scan-continue").hidden = running || scan.problems.length === 0;
  }

  function show() {
    clearTimeout(finishing);
    finishing = null;
    $("scan-page").hidden = false;
  }

  /** The scan is over: the page steps aside, and at startup the welcome page comes up. */
  function finish() {
    clearTimeout(finishing);
    finishing = null;
    const box = $("scan-page");
    if (box.contains(document.activeElement)) document.activeElement.blur();
    box.hidden = true;
    document.body.classList.remove("booting");
  }

  /** The answer to the question at startup: Yes, the scan (its page shows meanwhile); No, the
      welcome page straight away. */
  function choose(thirdParty) {
    if (chosen) return;
    chosen = true;
    $("plugin-choice").hidden = true;
    $("scan-box").hidden = false;
    Hub.send("choosePlugins", { thirdParty });
    if (!thirdParty) finish();
  }

  // A page loading again later (it shouldn't): the question was answered already.
  Hub.on("pluginChoice", () => {
    chosen = true;
    $("plugin-choice").hidden = true;
    $("scan-box").hidden = false;
  });

  Hub.on("pluginScan", (s) => {
    scan = { ...s, problems: s.problems || [] };

    if (s.state === "running") {
      show();
      render();
      return;
    }

    // Done or cancelled.
    const wasShowing = !$("scan-page").hidden;
    render();

    if (scan.problems.length) {
      // Nothing fails unseen: the list stays until Continue.
      show();
      $("scan-continue").focus();
    } else if (!wasShowing || s.total === 0) {
      finish();   // nothing was read (all up to date): straight on
    } else {
      finishing = setTimeout(finish, 700);   // a moment to see it finished
    }
  });

  Hub.scan = { state: () => scan, finish };

  Hub.on("ready", () => {
    render();
    $("choice-yes").addEventListener("click", () => choose(true));
    $("choice-no").addEventListener("click", () => choose(false));
    if (!chosen) $("choice-yes").focus();
    $("scan-skip").addEventListener("click", () => Hub.send("cancelScan"));
    $("scan-continue").addEventListener("click", finish);

    // While the page is up, keys don't reach the Hub behind it (Escape would close the welcome
    // page before it appeared); the page's own buttons still take Tab, Enter and Space.
    window.addEventListener("keydown", (event) => {
      const box = $("scan-page");
      if (box.hidden || (event.target instanceof Node && box.contains(event.target))) return;
      if (event.key === "Tab") return;
      event.preventDefault();
      event.stopImmediatePropagation();
    }, true);
  });
})();
