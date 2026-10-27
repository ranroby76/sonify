// D:\workspace\Fanan_web_hub\src\hub-guide.js
//
// The Guide studio (the top bar's selector, Ctrl+9): Sonify's user guide, and Fanan AI answering
// anything about Sonify.
//
// The guide is sonify_guide.md, compiled into Sonify with the page: "# " lines are the guide's
// tabs (Getting Started, Studio, Analog Board...), "## " lines the chapters in each tab, shown as
// a row of selectors over the chapter's text. Inside a chapter: "### " headings, "- " bullets,
// "1. " steps, "> " tips, and paragraphs (one line each); **bold** for the names of buttons,
// menus and keys. The search box finds chapters by their words, in every tab. The chapter last
// read opens again next time.
//
// "Ask anything about Sonify", on the right: a question goes to Sonify (a "guide" event:
// { action: "ask", text, reading }, reading being the chapter open here), which asks Fanan AI
// with the same guide (GuideAi), with the Fanan account's credits. Sonify sends "guideState":
// { busy, entries: [{ role: "user" | "ai" | "notice", text }] } and the account (configured,
// blocked, status, signedIn, credits, siteAvailable), as the AI panels have it. In an answer,
// [[Tab > Chapter]] is a link that opens that chapter.
//
// "Translate this page to": over the chapter's text, a list of languages (the ones Android has)
// and a Translate button. The open chapter's Markdown goes to Sonify ({ action: "translate", id,
// language, text }), which has Fanan AI translate it (GuideAi) and answers with "guideTranslation"
// { id, ok, text }. Translations are kept (per chapter, language and wording) so a page already
// translated shows again without credits. Save to PDF prints the translated page.

(() => {
  "use strict";

  const { $, element } = Hub;
  const PLACE_KEY = "sonify.guide.place";
  const EXAMPLES = [
    "How do I record my voice on a new track?",
    "How do I export my song as an audio file?",
    "How do I turn a plugin I made into a VST3 or CLAP plugin?",
  ];

  const LANG_KEY = "sonify.guide.lang", TRANSLATIONS_KEY = "sonify.guide.translations";
  const KEPT_TRANSLATIONS = 60;
  // [English name, the language's own name]: what Android offers, as far as a Latin-script request can name them.
  const LANGUAGES = [
    ["Afrikaans", "Afrikaans"], ["Albanian", "Shqip"], ["Amharic", "\u12a0\u121b\u122d\u129b"], ["Arabic", "\u0627\u0644\u0639\u0631\u0628\u064a\u0629"],
    ["Armenian", "\u0540\u0561\u0575\u0565\u0580\u0565\u0576"], ["Assamese", "\u0985\u09b8\u09ae\u09c0\u09af\u09bc\u09be"], ["Azerbaijani", "Az\u0259rbaycan"],
    ["Basque", "Euskara"], ["Belarusian", "\u0411\u0435\u043b\u0430\u0440\u0443\u0441\u043a\u0430\u044f"], ["Bengali", "\u09ac\u09be\u0982\u09b2\u09be"],
    ["Bosnian", "Bosanski"], ["Bulgarian", "\u0411\u044a\u043b\u0433\u0430\u0440\u0441\u043a\u0438"], ["Burmese", "\u1019\u103c\u1014\u103a\u1019\u102c"],
    ["Catalan", "Catal\u00e0"], ["Chinese (Simplified)", "\u7b80\u4f53\u4e2d\u6587"], ["Chinese (Traditional)", "\u7e41\u9ad4\u4e2d\u6587"],
    ["Croatian", "Hrvatski"], ["Czech", "\u010ce\u0161tina"], ["Danish", "Dansk"], ["Dutch", "Nederlands"], ["Estonian", "Eesti"],
    ["Filipino", "Filipino"], ["Finnish", "Suomi"], ["French", "Fran\u00e7ais"], ["Galician", "Galego"], ["Georgian", "\u10e5\u10d0\u10e0\u10d7\u10e3\u10da\u10d8"],
    ["German", "Deutsch"], ["Greek", "\u0395\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ac"], ["Gujarati", "\u0a97\u0ac1\u0a9c\u0ab0\u0abe\u0aa4\u0ac0"],
    ["Hebrew", "\u05e2\u05d1\u05e8\u05d9\u05ea"], ["Hindi", "\u0939\u093f\u0928\u094d\u0926\u0940"], ["Hungarian", "Magyar"], ["Icelandic", "\u00cdslenska"],
    ["Indonesian", "Bahasa Indonesia"], ["Irish", "Gaeilge"], ["Italian", "Italiano"], ["Japanese", "\u65e5\u672c\u8a9e"], ["Kannada", "\u0c95\u0ca8\u0ccd\u0ca8\u0ca1"],
    ["Kazakh", "\u049a\u0430\u0437\u0430\u049b\u0448\u0430"], ["Khmer", "\u1781\u17d2\u1798\u17c2\u179a"], ["Korean", "\ud55c\uad6d\uc5b4"], ["Kyrgyz", "\u041a\u044b\u0440\u0433\u044b\u0437\u0447\u0430"],
    ["Lao", "\u0ea5\u0eb2\u0ea7"], ["Latvian", "Latvie\u0161u"], ["Lithuanian", "Lietuvi\u0173"], ["Macedonian", "\u041c\u0430\u043a\u0435\u0434\u043e\u043d\u0441\u043a\u0438"],
    ["Malay", "Bahasa Melayu"], ["Malayalam", "\u0d2e\u0d32\u0d2f\u0d3e\u0d33\u0d02"], ["Marathi", "\u092e\u0930\u093e\u0920\u0940"], ["Mongolian", "\u041c\u043e\u043d\u0433\u043e\u043b"],
    ["Nepali", "\u0928\u0947\u092a\u093e\u0932\u0940"], ["Norwegian", "Norsk"], ["Odia", "\u0b13\u0b21\u0b3c\u0b3f\u0b06"], ["Persian", "\u0641\u0627\u0631\u0633\u06cc"],
    ["Polish", "Polski"], ["Portuguese (Brazil)", "Portugu\u00eas (Brasil)"], ["Portuguese (Portugal)", "Portugu\u00eas (Portugal)"], ["Punjabi", "\u0a2a\u0a70\u0a1c\u0a3e\u0a2c\u0a40"],
    ["Romanian", "Rom\u00e2n\u0103"], ["Russian", "\u0420\u0443\u0441\u0441\u043a\u0438\u0439"], ["Serbian", "\u0421\u0440\u043f\u0441\u043a\u0438"], ["Sinhala", "\u0dc3\u0dd2\u0d82\u0dc4\u0dbd"],
    ["Slovak", "Sloven\u010dina"], ["Slovenian", "Sloven\u0161\u010dina"], ["Spanish", "Espa\u00f1ol"], ["Swahili", "Kiswahili"], ["Swedish", "Svenska"],
    ["Tamil", "\u0ba4\u0bae\u0bbf\u0bb4\u0bcd"], ["Telugu", "\u0c24\u0c46\u0c32\u0c41\u0c17\u0c41"], ["Thai", "\u0e44\u0e17\u0e22"], ["Turkish", "T\u00fcrk\u00e7e"],
    ["Ukrainian", "\u0423\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430"], ["Urdu", "\u0627\u0631\u062f\u0648"], ["Uzbek", "O\u2018zbek"], ["Vietnamese", "Ti\u1ebfng Vi\u1ec7t"],
    ["Welsh", "Cymraeg"], ["Zulu", "isiZulu"],
  ];
  const RIGHT_TO_LEFT = ["Arabic", "Hebrew", "Persian", "Urdu"];

  let tabs = [];          // [{ name, chapters: [{ title, lines: [] }] }]
  let loaded = false, loading = false, failed = false;
  let tabIndex = 0, chapterIndex = 0;
  let shown = false;
  let language = "";      // the language chosen for "Translate this page to" (its English name), or ""
  let translated = null;  // the open chapter, translated: { tab, chapter, language, title, lines }
  let pending = null;     // the translation under way: { id, tab, chapter, language, key }
  let translationId = 0;
  let translationNote = "";   // why a translation failed, shown by the button
  let state = { busy: false, entries: [], configured: true, blocked: false, status: "checking",
                signedIn: false, credits: -1, siteAvailable: false };

  // ---------------------------------------------------------------- the guide's text
  function parse(text) {
    const result = [];
    let tab = null, chapter = null;

    for (const raw of text.split(/\r?\n/)) {
      const line = raw.replace(/\s+$/, "");
      if (line.trimStart().startsWith("<!--")) continue;   // the file's title

      if (line.startsWith("# ")) {
        tab = { name: line.slice(2).trim(), chapters: [] };
        result.push(tab);
        chapter = null;
      } else if (line.startsWith("## ")) {
        if (!tab) { tab = { name: "Guide", chapters: [] }; result.push(tab); }
        chapter = { title: line.slice(3).trim(), lines: [] };
        tab.chapters.push(chapter);
      } else if (chapter) {
        chapter.lines.push(line);
      }
    }

    return result.filter((t) => t.chapters.length);
  }

  /** A line's **bold** parts, as text and strong nodes. */
  function inline(parent, text) {
    const parts = text.split("**");
    const closed = parts.length % 2 === 1;   // an odd number of marks: the last one stays as it is

    parts.forEach((part, i) => {
      if (!part) return;
      const bold = i % 2 === 1 && (closed || i < parts.length - 1);
      if (bold) parent.appendChild(element("strong", "", part));
      else parent.appendChild(document.createTextNode(i % 2 === 1 ? "**" + part : part));
    });
  }

  function renderChapterText(article, tab, chapter) {
    article.replaceChildren();
    const inner = element("div", "guide-page-text");
    const view = translated && translated.tab === tabIndex && translated.chapter === chapterIndex ? translated : null;
    if (view && RIGHT_TO_LEFT.includes(view.language)) inner.dir = "rtl";
    else if (view) inner.dir = "auto";
    inner.appendChild(element("div", "guide-crumb", tab.name + (view ? " \u00b7 " + view.language : "")));
    inner.appendChild(element("h3", "guide-title", view ? view.title : chapter.title));
    chapter = view ? { title: view.title, lines: view.lines } : chapter;

    let list = null;
    const endList = () => { list = null; };

    for (const line of chapter.lines) {
      const trimmed = line.trim();

      if (!trimmed) { endList(); continue; }

      if (trimmed.startsWith("### ")) {
        endList();
        const heading = element("h4", "guide-heading");
        inline(heading, trimmed.slice(4));
        inner.appendChild(heading);
      } else if (/^[-*] /.test(trimmed)) {
        if (!list || list.tagName !== "UL") { list = element("ul", "guide-list"); inner.appendChild(list); }
        const item = element("li");
        inline(item, trimmed.slice(2));
        list.appendChild(item);
      } else if (/^\d+\. /.test(trimmed)) {
        if (!list || list.tagName !== "OL") { list = element("ol", "guide-steps"); inner.appendChild(list); }
        const item = element("li");
        inline(item, trimmed.replace(/^\d+\.\s+/, ""));
        list.appendChild(item);
      } else if (trimmed.startsWith("> ")) {
        endList();
        const tip = element("p", "guide-tip");
        const body = trimmed.slice(2).replace(/^Tip:\s*/i, "");
        tip.appendChild(element("span", "guide-tip-mark", "Tip"));
        inline(tip, body);
        inner.appendChild(tip);
      } else {
        endList();
        const paragraph = element("p", "guide-para");
        inline(paragraph, trimmed);
        inner.appendChild(paragraph);
      }
    }

    // On to the chapter before or after, across the tabs.
    const nav = element("div", "guide-nav");
    const before = neighbour(-1), after = neighbour(1);
    if (before) nav.appendChild(navButton("\u2190 " + before.label, before.tab, before.chapter));
    nav.appendChild(element("span", "guide-nav-gap"));
    if (after) nav.appendChild(navButton(after.label + " \u2192", after.tab, after.chapter));
    inner.appendChild(nav);

    article.appendChild(inner);
    article.scrollTop = 0;
  }

  function neighbour(step) {
    let t = tabIndex, c = chapterIndex + step;
    if (c < 0) { t -= 1; if (t < 0) return null; c = tabs[t].chapters.length - 1; }
    else if (c >= tabs[t].chapters.length) { t += 1; if (t >= tabs.length) return null; c = 0; }
    const label = t === tabIndex ? tabs[t].chapters[c].title : tabs[t].name + ": " + tabs[t].chapters[c].title;
    return { tab: t, chapter: c, label };
  }

  function navButton(text, tab, chapter) {
    const button = element("button", "plain-button small guide-nav-button", text);
    button.type = "button";
    button.addEventListener("click", () => open(tab, chapter));
    return button;
  }

  // ---------------------------------------------------------------- tabs, selectors and the page
  function renderTabs() {
    const row = $("guide-tabs");
    row.replaceChildren();

    tabs.forEach((tab, i) => {
      const button = element("button", "guide-tab", tab.name);
      button.type = "button";
      button.setAttribute("role", "tab");
      button.setAttribute("aria-selected", String(i === tabIndex));
      button.tabIndex = i === tabIndex ? 0 : -1;
      button.addEventListener("click", () => open(i, 0));
      row.appendChild(button);
    });
  }

  function renderChapters() {
    const grid = $("guide-chapters");
    grid.replaceChildren();
    const tab = tabs[tabIndex];
    if (!tab) return;

    tab.chapters.forEach((chapter, i) => {
      const button = element("button", "guide-chapter");
      button.type = "button";
      button.setAttribute("role", "tab");
      button.setAttribute("aria-selected", String(i === chapterIndex));
      button.tabIndex = i === chapterIndex ? 0 : -1;
      button.append(element("span", "guide-chapter-number", String(i + 1)), element("span", "guide-chapter-name", chapter.title));
      button.addEventListener("click", () => open(tabIndex, i));
      grid.appendChild(button);
    });
  }

  function renderPage() {
    const article = $("guide-text");

    if (!loaded) {
      article.replaceChildren(element("p", "guide-empty", failed ? "The guide couldn't be loaded." : "Loading the guide\u2026"));
      return;
    }

    const query = $("guide-search").value.trim();
    renderTabs();
    renderChapters();

    if (query) renderResults(article, query);
    else renderChapterText(article, tabs[tabIndex], tabs[tabIndex].chapters[chapterIndex]);
    renderTranslate();
  }

  /** Opens a chapter (the search, if any, is done with). */
  function open(tab, chapter) {
    if (!tabs[tab] || !tabs[tab].chapters[chapter]) return;
    tabIndex = tab;
    chapterIndex = chapter;
    $("guide-search").value = "";
    translated = cachedTranslation(tab, chapter);   // a chapter already translated into the chosen language shows so again
    translationNote = "";
    try { localStorage.setItem(PLACE_KEY, JSON.stringify({ tab: tabs[tab].name, chapter: tabs[tab].chapters[chapter].title })); } catch (e) { /* not kept */ }
    renderPage();
  }

  function plainName(text) {
    return String(text).replace(/[\[\]"*]/g, "").replace(/\s+/g, " ").trim().toLowerCase();
  }

  /** A chapter by its name, "Tab > Chapter" (or a chapter's title, or a tab's): [tab, chapter] or null. */
  function find(name) {
    const wanted = plainName(name);
    if (!wanted) return null;

    if (wanted.includes(">")) {
      const at = wanted.indexOf(">");
      const tab = wanted.slice(0, at).trim(), title = wanted.slice(at + 1).trim();
      for (let t = 0; t < tabs.length; t++)
        for (let c = 0; c < tabs[t].chapters.length; c++)
          if (plainName(tabs[t].name) === tab && plainName(tabs[t].chapters[c].title) === title) return [t, c];
      return find(title);
    }

    for (let t = 0; t < tabs.length; t++)
      for (let c = 0; c < tabs[t].chapters.length; c++)
        if (plainName(tabs[t].chapters[c].title) === wanted) return [t, c];

    for (let t = 0; t < tabs.length; t++)
      if (plainName(tabs[t].name) === wanted) return [t, 0];

    return null;
  }

  function openByName(name) {
    const found = find(name);
    if (found) open(found[0], found[1]);
    return Boolean(found);
  }

  function readingName() {
    const tab = tabs[tabIndex];
    const chapter = tab && tab.chapters[chapterIndex];
    return chapter && !$("guide-search").value.trim() ? tab.name + " > " + chapter.title : "";
  }

  // ---------------------------------------------------------------- the search
  function renderResults(article, query) {
    article.replaceChildren();
    const inner = element("div", "guide-page-text");
    const words = query.toLowerCase().split(/\s+/).filter(Boolean);
    const found = [];

    tabs.forEach((tab, t) => tab.chapters.forEach((chapter, c) => {
      const title = chapter.title.toLowerCase();
      const body = chapter.lines.join("\n").replace(/\*\*/g, "").toLowerCase();
      if (!words.every((word) => title.includes(word) || body.includes(word) || tab.name.toLowerCase().includes(word))) return;
      const score = words.reduce((sum, word) => sum + (title.includes(word) ? 10 : 0) + Math.min(5, body.split(word).length - 1), 0);
      const line = chapter.lines.find((l) => l.toLowerCase().includes(words[0])) || chapter.lines.find((l) => l.trim()) || "";
      found.push({ t, c, score, line: line.replace(/^\s*(#+|[-*]|\d+\.|>)\s*/, "").replace(/\*\*/g, "") });
    }));

    found.sort((a, b) => b.score - a.score);
    inner.appendChild(element("div", "guide-crumb", "Search"));
    inner.appendChild(element("h3", "guide-title", found.length ? `${found.length} ${found.length === 1 ? "chapter" : "chapters"} for "${query}"`
                                                               : `Nothing in the guide for "${query}"`));
    if (!found.length) inner.appendChild(element("p", "guide-para", "Try other words, or ask Fanan AI on the right."));

    for (const result of found.slice(0, 40)) {
      const tab = tabs[result.t], chapter = tab.chapters[result.c];
      const button = element("button", "guide-result");
      button.type = "button";
      button.appendChild(element("span", "guide-result-name", tab.name + " > " + chapter.title));
      const snippet = result.line.length > 180 ? result.line.slice(0, 177) + "..." : result.line;
      button.appendChild(element("span", "guide-result-line", snippet));
      button.addEventListener("click", () => open(result.t, result.c));
      inner.appendChild(button);
    }

    article.appendChild(inner);
    article.scrollTop = 0;
  }

  // ---------------------------------------------------------------- Translate this page to
  function chapterMarkdown(chapter) {
    return "## " + chapter.title + "\n" + chapter.lines.join("\n").replace(/\s+$/, "");
  }

  /** A short fingerprint of the chapter's English text: a changed chapter is translated afresh. */
  function fingerprint(text) {
    let hash = 5381;
    for (let i = 0; i < text.length; i++) hash = ((hash * 33) ^ text.charCodeAt(i)) >>> 0;
    return hash.toString(36);
  }

  function translationKey(tab, chapter, lang) {
    const c = tabs[tab].chapters[chapter];
    return [tabs[tab].name, c.title, lang, fingerprint(chapterMarkdown(c))].join("|");
  }

  function readTranslations() {
    try { return JSON.parse(localStorage.getItem(TRANSLATIONS_KEY) || "[]"); } catch (e) { return []; }
  }

  /** Splits a translated page into its title and lines. */
  function splitTranslation(text) {
    const lines = String(text).replace(/\r/g, "").split("\n").filter((l) => !l.trimStart().startsWith("```"));
    while (lines.length && !lines[0].trim()) lines.shift();
    let title = "";
    if (lines.length && /^#{1,2}\s/.test(lines[0])) title = lines.shift().replace(/^#+\s*/, "").trim();
    return { title, lines: lines.map((l) => l.replace(/\s+$/, "")) };
  }

  function cachedTranslation(tab, chapter) {
    if (!language || !tabs[tab] || !tabs[tab].chapters[chapter]) return null;
    const key = translationKey(tab, chapter, language);
    const found = readTranslations().find((item) => item.key === key);
    if (!found) return null;
    const parts = splitTranslation(found.text);
    return { tab, chapter, language, title: parts.title || tabs[tab].chapters[chapter].title, lines: parts.lines };
  }

  function keepTranslation(key, text) {
    try {
      const list = readTranslations().filter((item) => item.key !== key);
      list.push({ key, text });
      while (list.length > KEPT_TRANSLATIONS) list.shift();
      localStorage.setItem(TRANSLATIONS_KEY, JSON.stringify(list));
    } catch (e) { /* not kept: it shows now, and is asked again next time */ }
  }

  function startTranslation() {
    if (!loaded || !language || pending || $("guide-search").value.trim()) return;
    const key = translationKey(tabIndex, chapterIndex, language);
    const cached = cachedTranslation(tabIndex, chapterIndex);
    translationNote = "";

    if (cached) { translated = cached; renderPage(); return; }

    pending = { id: ++translationId, tab: tabIndex, chapter: chapterIndex, language, key };
    const id = pending.id;
    setTimeout(() => {   // an answer that never comes doesn't leave the button waiting
      if (pending && pending.id === id) {
        pending = null;
        translationNote = "The translation didn't come. Please try again.";
        renderTranslate();
      }
    }, 240000);
    Hub.send("guide", { action: "translate", id: pending.id, language, text: chapterMarkdown(tabs[tabIndex].chapters[chapterIndex]) });
    renderTranslate();
  }

  Hub.on("guideTranslation", (data) => {
    if (!pending || data.id !== pending.id) return;
    const job = pending;
    pending = null;

    if (!data.ok) {
      translationNote = String(data.text || "The translation didn't come. Please try again.");
      renderTranslate();
      return;
    }

    keepTranslation(job.key, data.text);
    const parts = splitTranslation(data.text);
    const same = job.tab === tabIndex && job.chapter === chapterIndex && job.language === language;
    if (same) {
      translated = { tab: job.tab, chapter: job.chapter, language: job.language, title: parts.title || tabs[job.tab].chapters[job.chapter].title, lines: parts.lines };
      renderPage();
    } else {
      renderTranslate();   // the reader moved on: it shows when that chapter is opened again
    }
  });

  function renderTranslate() {
    const select = $("guide-lang");
    if (!select.options.length) {
      const first = element("option", "", "Choose a language");
      first.value = "";
      select.appendChild(first);
      for (const [english, own] of LANGUAGES) {
        const option = element("option", "", english === own ? english : english + " \u2014 " + own);
        option.value = english;
        select.appendChild(option);
      }
    }
    select.value = language;

    const searching = Boolean($("guide-search").value.trim());
    const [reason] = whyNot();
    const showing = Boolean(translated && translated.tab === tabIndex && translated.chapter === chapterIndex);
    const working = Boolean(pending);

    $("guide-translate-go").disabled = !language || working || searching || (Boolean(reason) && !cachedTranslation(tabIndex, chapterIndex));
    $("guide-translate-go").textContent = working ? "Translating\u2026" : "Translate";
    $("guide-translate-original").hidden = !showing;
    $("guide-translate-pdf").hidden = !showing;
    select.disabled = working;

    const note = $("guide-translate-note");
    note.classList.toggle("guide-translate-error", Boolean(translationNote));
    note.textContent = translationNote || (working ? "Fanan AI is translating this page\u2026"
                                           : reason && language && !showing ? reason : "Uses Fanan AI credits.");
  }

  /** The translated page, on its own, to the printer: Save as PDF is among the printer choices. */
  function savePdf() {
    const text = document.querySelector("#guide-text .guide-page-text");
    if (!text) return;
    const sheet = element("div");
    sheet.id = "guide-print";
    const copy = text.cloneNode(true);
    copy.querySelectorAll(".guide-nav").forEach((node) => node.remove());
    sheet.appendChild(copy);
    document.body.appendChild(sheet);
    const clean = () => sheet.remove();
    window.addEventListener("afterprint", clean, { once: true });
    setTimeout(() => window.print(), 50);
    setTimeout(clean, 120000);
  }

  // ---------------------------------------------------------------- Ask anything about Sonify
  /** A line's inline marks, each only when closed on the same line: **bold**, *italic*, `code`
      (shown as plain words), and [[Tab > Chapter]] (a link to that chapter). */
  function inlineAnswer(parent, line, bold) {
    const pieces = line.split(/(\[\[[^\]]+\]\])/);

    for (const piece of pieces) {
      const link = /^\[\[([^\]]+)\]\]$/.exec(piece);

      if (link) {
        const name = link[1].trim();
        if (find(name)) {
          const button = element("button", "guide-link", name);
          button.type = "button";
          button.title = "Open this chapter of the guide";
          button.addEventListener("click", () => openByName(name));
          parent.appendChild(button);
        } else {
          parent.appendChild(element("span", bold ? "ai-bold" : "", name));
        }
        continue;
      }

      let italic = false, code = false, current = "";
      const flush = () => {
        if (current) parent.appendChild(element("span", [bold && "ai-bold", italic && "ai-italic"].filter(Boolean).join(" "), current));
        current = "";
      };
      const closedLater = (mark, from) => piece.indexOf(mark, from) >= 0;

      for (let i = 0; i < piece.length;) {
        if (!code && piece.startsWith("**", i) && (bold || closedLater("**", i + 2))) { flush(); bold = !bold; i += 2; }
        else if (piece[i] === "`" && (code || closedLater("`", i + 1))) { flush(); code = !code; i += 1; }
        else if (!code && piece[i] === "*" && (italic || (i + 1 < piece.length && piece[i + 1] !== " " && closedLater("*", i + 1)))) { flush(); italic = !italic; i += 1; }
        else { current += piece[i]; i += 1; }
      }
      flush();
    }
  }

  function renderAnswer(text) {
    const lines = [];
    let inCode = false;
    for (const line of text.split(/\r?\n/)) {
      if (line.trimStart().startsWith("```")) inCode = !inCode;
      else if (!inCode) lines.push(line);
    }
    while (lines.length && !lines[lines.length - 1].trim()) lines.pop();

    const box = element("div", "ai-text");

    for (const original of lines) {
      let line = original;
      const trimmed = line.trimStart();
      let kind = "";

      if (trimmed === "---" || trimmed === "***" || trimmed === "___") line = "";
      else if (trimmed.startsWith("#")) { line = trimmed.replace(/^#+\s*/, ""); kind = " heading"; }
      else if (/^([-*]|\u2022) /.test(trimmed)) { line = trimmed.slice(2); kind = " bullet"; }
      else if (trimmed.startsWith("> ")) line = trimmed.slice(2);

      const row = element("div", "ai-line" + kind);
      row.dir = "auto";   // Hebrew, Arabic...: right to left
      inlineAnswer(row, line, kind === " heading");
      box.appendChild(row);
    }

    return box;
  }

  /** [why Fanan AI can't answer right now, what a button does about it, the button's text]. */
  function whyNot() {
    const tryAgain = () => Hub.send("accountCheck");
    if (!state.configured) return ["Fanan AI isn't set up in this build yet."];
    if (state.blocked) return ["The internet is blocked (top left): Fanan AI works through it."];
    if (state.status === "checking") return ["Connecting to Fanan's server\u2026"];
    if (state.status === "noInternet") return ["No internet connection: Fanan AI works through it.", tryAgain, "Try again"];
    if (state.status !== "online") return ["Fanan's server isn't responding right now.", tryAgain, "Try again"];
    if (!state.signedIn) return ["Sign in to ask Fanan AI. It's your Fanan account: its Fanan AI credits work in every Fanan app.", () => Hub.run("account"), "Sign in"];
    if (state.credits === 0) return ["You're out of credits.", () => Hub.run("buyCredits"), "Buy credits"];
    return [""];
  }

  function renderAsk() {
    const log = $("guide-ask-log");
    const nearBottom = log.scrollHeight - log.scrollTop - log.clientHeight < 40;
    log.replaceChildren();

    if (!state.entries.length) {
      const hello = element("div", "ai-hello");
      hello.appendChild(element("p", "", "Ask anything about Sonify: how to do something, where to find it, what a studio is for. "
                                          + "Fanan AI answers from this guide, in your language, and links the chapters to read."));
      for (const text of EXAMPLES) {
        const example = element("button", "ai-example", text);
        example.type = "button";
        example.addEventListener("click", () => { $("guide-ask-input").value = text; $("guide-ask-input").focus(); });
        hello.appendChild(example);
      }
      hello.appendChild(element("p", "guide-ask-cost", "Each question uses Fanan AI credits, like any Fanan AI request."));
      log.appendChild(hello);
    }

    for (const entry of state.entries) {
      if (entry.role === "ai") {
        const box = element("div", "ai-assistant");
        box.appendChild(renderAnswer(entry.text || ""));
        log.appendChild(box);
      } else {
        const line = element("div", entry.role === "user" ? "ai-user" : "ai-error", entry.text);
        line.dir = "auto";
        log.appendChild(line);
      }
    }

    if (state.busy) log.appendChild(element("div", "ai-working", "Fanan AI is reading the guide\u2026"));
    if (nearBottom || state.busy) log.scrollTop = log.scrollHeight;

    const [reason, action, actionText] = whyNot();
    $("guide-ask-notice").hidden = !reason;
    $("guide-ask-notice-text").textContent = reason;
    $("guide-ask-notice-action").hidden = !action;
    $("guide-ask-notice-action").textContent = actionText || "";
    $("guide-ask-notice-action").onclick = action || null;
    $("guide-ask-input").disabled = Boolean(reason);
    $("guide-ask-send").disabled = Boolean(reason);
    $("guide-ask-send").hidden = state.busy;
    $("guide-ask-stop").hidden = !state.busy;
    $("guide-ask-clear").disabled = state.busy || !state.entries.length;

    const credits = $("guide-ask-credits");
    credits.replaceChildren();
    if (state.signedIn && state.credits >= 0) {
      credits.appendChild(document.createTextNode(`${state.credits} ${state.credits === 1 ? "credit" : "credits"}`));
      if (state.siteAvailable) {
        const buy = element("button", "ai-buy", "Buy credits");
        buy.type = "button";
        buy.title = "Buy Fanan AI credits on the Fanan site: they work in every Fanan app";
        buy.addEventListener("click", () => Hub.run("buyCredits"));
        credits.append(" \u00b7 ", buy);
      }
    }
  }

  function ask() {
    const input = $("guide-ask-input");
    const text = input.value.trim();
    if (!text || state.busy || input.disabled) return;
    Hub.send("guide", { action: "ask", text, reading: readingName() });
    input.value = "";
  }

  // ---------------------------------------------------------------- loading and showing
  function restorePlace() {
    try {
      const place = JSON.parse(localStorage.getItem(PLACE_KEY) || "null");
      const found = place && find(place.tab + " > " + place.chapter);
      if (found) { tabIndex = found[0]; chapterIndex = found[1]; }
    } catch (e) { /* the first chapter, then */ }
  }

  function load() {
    if (loaded || loading) return;
    loading = true;
    failed = false;

    fetch("sonify_guide.md")
      .then((response) => {
        if (!response.ok) throw new Error("status " + response.status);
        return response.text();
      })
      .then((text) => {
        tabs = parse(text);
        if (!tabs.length) throw new Error("empty");
        loaded = true;
        restorePlace();
        translated = cachedTranslation(tabIndex, chapterIndex);
      })
      .catch((error) => {
        failed = true;
        console.error("Guide: the guide couldn't be loaded", error);
      })
      .finally(() => {
        loading = false;
        renderPage();
      });
  }

  function show(on) {
    shown = on;
    if (!on) return;
    load();
    renderPage();
    renderAsk();
    Hub.send("guide", { action: "state" });
  }

  Hub.guide = { show, open: openByName, isShown: () => shown };

  // Sonify's answers (an event the page's own list doesn't name, so it's listened to here).
  if (window.__JUCE__ && window.__JUCE__.backend)
    window.__JUCE__.backend.addEventListener("guideState", (data) => Hub.emit("guideState", data));

  if (window.__JUCE__ && window.__JUCE__.backend)
    window.__JUCE__.backend.addEventListener("guideTranslation", (data) => Hub.emit("guideTranslation", data));

  Hub.on("guideState", (data) => {
    state = { ...state, ...data };
    renderAsk();
    renderTranslate();
  });

  /** The arrow keys move along a row of tabs or selectors while it has the focus. */
  function arrowKeys(row, move) {
    row.addEventListener("keydown", (event) => {
      if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
      event.preventDefault();
      event.stopPropagation();
      move(event.key === "ArrowLeft" ? -1 : 1);
      const selected = row.querySelector('[aria-selected="true"]');
      if (selected) selected.focus();
    });
  }

  Hub.on("ready", () => {
    const input = $("guide-ask-input");
    input.dir = "auto";
    $("guide-ask-send").addEventListener("click", ask);
    $("guide-ask-stop").addEventListener("click", () => Hub.send("guide", { action: "stop" }));
    $("guide-ask-clear").addEventListener("click", () => Hub.send("guide", { action: "clear" }));
    input.addEventListener("keydown", (event) => {
      if (event.key === "Enter" && !event.shiftKey) {
        event.preventDefault();
        ask();
      }
    });

    try { language = localStorage.getItem(LANG_KEY) || ""; } catch (e) { language = ""; }
    if (!LANGUAGES.some((item) => item[0] === language)) language = "";
    $("guide-lang").addEventListener("change", () => {
      language = $("guide-lang").value;
      try { localStorage.setItem(LANG_KEY, language); } catch (e) { /* not kept */ }
      translationNote = "";
      translated = language ? cachedTranslation(tabIndex, chapterIndex) : null;
      renderPage();
    });
    $("guide-translate-go").addEventListener("click", startTranslation);
    $("guide-translate-original").addEventListener("click", () => { translated = null; translationNote = ""; renderPage(); });
    $("guide-translate-pdf").addEventListener("click", savePdf);

    const search = $("guide-search");
    search.addEventListener("input", () => { if (loaded) renderPage(); });
    search.addEventListener("keydown", (event) => {
      if (event.key === "Escape" && search.value) {
        event.preventDefault();
        event.stopPropagation();
        search.value = "";
        renderPage();
      }
    });

    arrowKeys($("guide-tabs"), (step) => { if (tabs.length) open((tabIndex + step + tabs.length) % tabs.length, 0); });
    arrowKeys($("guide-chapters"), (step) => {
      const count = tabs[tabIndex] ? tabs[tabIndex].chapters.length : 0;
      if (count) open(tabIndex, (chapterIndex + step + count) % count);
    });

    renderAsk();
  });
})();
