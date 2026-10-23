// D:\workspace\Fanan_web_hub\src\hub-web.js
//
// The Web Browser studio (the top bar's selector, Ctrl+8): the internet in tabs of its own,
// while the song plays on (the transport stays below).
//   Under the top bar: its tabs, newest last, then + Open new tab; under them the browser's
//   buttons (Back, Forward, Reload, Home) and the address bar: a web address, or words to
//   search for. A new tab shows quick links until it's given an address.
//   Each tab is a real browser view (WebView2) laid over the content area while the Web Browser
//   is in front; in the other studios the view steps aside, and the tabs stay as they were.
//   A page that opens a new window opens it as a tab here, and the Web Browser comes to the front.
// Keys: Ctrl+T a new tab (from any studio; in the Studio, unless the keyboard map gives Ctrl+T a
// command), and in the Web Browser: Ctrl+W close the tab, Ctrl+PageUp / Ctrl+PageDown the
// previous / next tab, Ctrl+L the address bar, Alt+Left / Alt+Right back and forward, F5 or
// Ctrl+R reload.

(() => {
  "use strict";

  const { $, element } = Hub;
  const tabs = [];            // { id, url, title, loading, error }
  let current = 0;            // the tab showing (0: none)
  let shown = false;          // the Web Browser is in front

  const SEARCH = "https://www.google.com/search?q=";
  const QUICK = [["YouTube", "https://www.youtube.com"], ["KVR Audio", "https://www.kvraudio.com"],
                 ["Bandcamp", "https://bandcamp.com"], ["Wikipedia", "https://www.wikipedia.org"]];

  /** What was typed, as an address: a URL as it is, a bare address with https://, words as a search. */
  function toUrl(text) {
    const typed = text.trim();
    if (!typed) return "";
    if (/^[a-z][a-z0-9+.-]*:\/\//i.test(typed) || /^(about|data):/i.test(typed)) return typed;
    if (!/\s/.test(typed) && (/^localhost(:\d+)?(\/.*)?$/i.test(typed) || /^[\w-]+(\.[\w-]+)+(:\d+)?([/?#].*)?$/.test(typed))) return "https://" + typed;
    return SEARCH + encodeURIComponent(typed);
  }

  const tabOf = (id) => tabs.find((t) => t.id === id) || null;
  const labelOf = (tab) => tab.title || (tab.url ? tab.url.replace(/^https?:\/\/(www\.)?/, "").replace(/\/$/, "") : "New tab");

  // ---------------------------------------------------------------- the tabs
  function renderStrip() {
    const strip = $("web-tab-strip");
    strip.textContent = "";

    for (const tab of tabs) {
      const button = element("button", "web-tab" + (tab.loading ? " loading" : ""));
      button.type = "button";
      button.dataset.tab = String(tab.id);
      button.setAttribute("role", "tab");
      button.setAttribute("aria-selected", String(tab.id === current));
      button.title = tab.url || "New tab";
      button.appendChild(element("span", "web-tab-title", labelOf(tab)));
      const shut = element("span", "web-tab-close", "×");
      shut.setAttribute("role", "button");
      shut.setAttribute("aria-label", `Close ${labelOf(tab)}`);
      button.appendChild(shut);
      strip.appendChild(button);
    }

    const active = strip.querySelector('[aria-selected="true"]');
    if (active) active.scrollIntoView({ block: "nearest", inline: "nearest" });
  }

  // ---------------------------------------------------------------- the page area
  /** Where the active tab's view goes (in CSS pixels, which are the window's own), or none. */
  function placeView() {
    const tab = tabOf(current);
    const overlay = [...document.querySelectorAll(".modal"), $("start-page"), $("scan-page")].some((box) => box && !box.hidden);

    if (!shown || !tab || !tab.url || overlay || document.querySelector(".popup-menu")) {
      Hub.send("webTabHide");
      return;
    }

    const r = $("web-content").getBoundingClientRect();
    Hub.send("webTabShow", { id: tab.id, x: Math.round(r.left), y: Math.round(r.top), width: Math.round(r.width), height: Math.round(r.height) });
  }

  function renderWebArea() {
    const tab = tabOf(current);
    const address = $("web-address");
    if (document.activeElement !== address) address.value = tab ? tab.url : "";

    const loading = Boolean(tab && tab.loading);
    $("web-reload").textContent = loading ? "✕" : "⟳";
    $("web-reload").title = loading ? "Stop" : "Reload (F5)";
    $("web-reload").setAttribute("aria-label", loading ? "Stop" : "Reload");
    for (const id of ["web-back", "web-forward", "web-reload"]) $(id).disabled = !tab || !tab.url;

    $("web-start").hidden = Boolean(tab && tab.url);
    $("web-error").hidden = !(tab && tab.error);
    $("web-error").textContent = tab && tab.error ? `This page couldn't load: ${tab.error}` : "";
  }

  function render() {
    renderStrip();
    renderWebArea();
    placeView();
  }

  /** Shows a tab of the Web Browser (it comes to the front if it isn't). */
  function activateTab(id) {
    const tab = tabOf(id);
    if (!tab) return;
    current = tab.id;

    if (!shown && Hub.studios) {
      Hub.studios.activate("web");   // shows it: show() renders
      return;
    }

    render();
    if (!tab.url) focusAddress();
  }

  function focusAddress() {
    const address = $("web-address");
    address.focus();
    address.select();
  }

  function newTab(url = "") {
    if (Hub.netBlocked && Hub.netBlocked() && url) {
      Hub.toast("The internet is blocked (top left): web pages can't load.");
      return;
    }
    Hub.send("webTabNew", { url });
  }

  function navigate(text) {
    const url = toUrl(text);
    if (!url) return;

    const tab = tabOf(current);
    if (!tab) {
      newTab(url);
      return;
    }

    tab.url = url;
    tab.error = "";
    tab.loading = true;
    Hub.send("webTabNavigate", { id: tab.id, url });
    $("web-address").blur();
    render();
  }

  function closeTab(id) {
    if (tabOf(id)) Hub.send("webTabClose", { id });
  }

  function cycleTabs(step) {
    if (tabs.length < 2) return;
    const at = tabs.findIndex((t) => t.id === current);
    activateTab(tabs[(at + step + tabs.length) % tabs.length].id);
  }

  function command(name) {
    const tab = tabOf(current);
    if (tab && tab.url) Hub.send("webTabCommand", { id: tab.id, command: name });
  }

  /** The Web Browser in front (on) or not. It always has a tab: a new one when there's none. */
  function show(on) {
    if (on === shown) return;
    shown = on;

    if (!on) {
      Hub.send("webTabHide");
      return;
    }

    if (!tabs.length) {
      newTab();
      return;   // the new tab shows when Sonify says it's open
    }

    if (!tabOf(current)) current = tabs[tabs.length - 1].id;
    render();
    const tab = tabOf(current);
    if (tab && !tab.url) focusAddress();
  }

  // ---------------------------------------------------------------- from the engine
  Hub.on("webTabOpened", (data) => {
    tabs.push({ id: data.id, url: data.url || "", title: data.title || "", loading: Boolean(data.url), error: "" });
    if (data.activate || !tabOf(current)) activateTab(data.id);
    else renderStrip();
  });

  Hub.on("webTabsRestore", (data) => {
    for (const saved of data.tabs || [])
      if (!tabOf(saved.id)) tabs.push({ id: saved.id, url: saved.url, title: saved.title || "", loading: false, error: "" });

    if (!tabOf(current) && tabs.length) current = tabs[tabs.length - 1].id;
    if (shown) render();
    else renderStrip();
  });

  Hub.on("webTabState", (data) => {
    const tab = tabOf(data.id);
    if (!tab) return;
    tab.url = data.url || tab.url;
    tab.loading = Boolean(data.loading);
    if (data.title) tab.title = data.title;
    if (data.loading) tab.error = "";
    if (data.error) tab.error = data.error;
    renderStrip();
    if (tab.id === current) renderWebArea();
  });

  Hub.on("webTabClosed", (data) => {
    const index = tabs.findIndex((t) => t.id === data.id);
    if (index < 0) return;
    tabs.splice(index, 1);

    if (current === data.id) current = tabs.length ? tabs[Math.min(index, tabs.length - 1)].id : 0;

    // The last tab closed while the Web Browser shows: a new one takes its place, as browsers do.
    if (shown && !tabs.length) {
      newTab();
      render();
      return;
    }

    render();
  });

  // ---------------------------------------------------------------- commands
  Hub.commands.newTab = () => {
    if (Hub.studios && Hub.studios.active() !== "web") Hub.studios.activate("web");
    if (tabs.length) newTab();   // the Web Browser makes its first tab itself when it has none
  };
  Hub.commands.closeTab = () => { if (shown && current) closeTab(current); };

  Hub.web = { show, isShown: () => shown, tabs, toUrl, newTab };

  // ---------------------------------------------------------------- the page's parts
  Hub.on("ready", () => {
    $("web-new-tab").addEventListener("click", () => newTab());

    const strip = $("web-tab-strip");
    strip.addEventListener("click", (event) => {
      const button = event.target.closest(".web-tab");
      if (!button) return;
      const id = Number(button.dataset.tab);
      if (event.target.closest(".web-tab-close")) closeTab(id);
      else activateTab(id);
    });
    strip.addEventListener("auxclick", (event) => {
      const button = event.target.closest(".web-tab");
      if (button && event.button === 1) closeTab(Number(button.dataset.tab));
    });

    $("web-back").addEventListener("click", () => command("back"));
    $("web-forward").addEventListener("click", () => command("forward"));
    $("web-reload").addEventListener("click", () => {
      const tab = tabOf(current);
      if (tab) command(tab.loading ? "stop" : "reload");
    });
    $("web-home").addEventListener("click", () => {
      // Home: a fresh tab page in place of this one.
      const tab = tabOf(current);
      if (tab && tab.url) {
        closeTab(tab.id);
        newTab();
      } else {
        focusAddress();
      }
    });

    const address = $("web-address");
    address.addEventListener("keydown", (event) => {
      if (event.key === "Enter") { event.preventDefault(); navigate(event.target.value); }
      if (event.key === "Escape") { const tab = tabOf(current); event.target.value = tab ? tab.url : ""; event.target.blur(); }
    });
    address.addEventListener("focus", (event) => event.target.select());

    const quick = $("web-quick");
    for (const [name, url] of QUICK) {
      const link = element("button", "web-quick-link", name);
      link.type = "button";
      link.title = url;
      link.addEventListener("click", () => navigate(url));
      quick.appendChild(link);
    }

    // The view follows the content area as the window changes.
    if (window.ResizeObserver) new ResizeObserver(() => { if (shown) placeView(); }).observe($("web-content"));
    window.addEventListener("resize", () => { if (shown) placeView(); });

    // A dialog or a menu opened over the Web Browser needs the page in front: the view steps aside meanwhile.
    const watch = new MutationObserver(() => { if (shown) placeView(); });
    for (const box of [...document.querySelectorAll(".modal"), $("start-page"), $("scan-page")])
      if (box) watch.observe(box, { attributes: true, attributeFilter: ["hidden"] });
    watch.observe(document.body, { childList: true });

    window.addEventListener("keydown", (event) => {
      const ctrl = event.ctrlKey || event.metaKey;
      if (!$("scan-page").hidden || document.querySelector(".modal:not([hidden])")) return;

      // Ctrl+T from any studio; in the Studio, unless the keyboard map gives it a command (Logic splits with it).
      if (ctrl && !event.altKey && !event.shiftKey && event.code === "KeyT") {
        const studio = !Hub.studios || Hub.studios.active() === "studio";
        if (studio && Hub.keys && Hub.keys.isBound(event)) return;
        event.preventDefault();
        event.stopImmediatePropagation();
        Hub.run("newTab");
        return;
      }

      if (!shown) return;
      let handled = true;

      if (ctrl && !event.altKey && event.code === "KeyW") { if (current) closeTab(current); }
      else if (ctrl && event.code === "PageUp") cycleTabs(-1);
      else if (ctrl && event.code === "PageDown") cycleTabs(1);
      else if (ctrl && !event.altKey && event.code === "KeyL") focusAddress();
      else if (event.altKey && event.code === "ArrowLeft") command("back");
      else if (event.altKey && event.code === "ArrowRight") command("forward");
      else if (event.code === "F5" || (ctrl && event.code === "KeyR")) command("reload");
      else handled = false;

      if (handled) {
        event.preventDefault();
        event.stopImmediatePropagation();
      }
    }, true);

    renderStrip();
    renderWebArea();
  });
})();
