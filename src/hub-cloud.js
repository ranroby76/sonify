// D:\workspace\Fanan_web_hub\src\hub-cloud.js
//
// The Fanan account: the same account, and the same credits, as Stella AI Studio.
//   The account button (top right) opens it. Signing in happens on the Fanan site, in the
//   browser: Sonify asks Fanan's server for a one-time link, the site signs the user in (or
//   makes the account) and links this computer. From then on Sonify is signed in by itself
//   at every start, until the user signs out. No password is ever typed here.
//   The email box knows the email last signed in with on this computer: it's suggested on
//   hover (a click takes it), completed as it's typed, and greeted once it's there.
//   Fanan's content (the Browser's Fanan tab) needs no account: that's hub-content.js.

(() => {
  "use strict";

  const { $ } = Hub;
  let account = { configured: true, siteAvailable: true, status: "checking", statusText: "Connecting…", signedIn: false,
                  email: "", lastEmail: "", credits: -1, computer: "", busy: false, waiting: false };
  let askingSignOut = false;   // "Sign out?" is showing
  let signingOut = false;
  let typedEmail = "";         // what was last sent to sign in with (Open the browser again uses it)

  const online = () => account.status === "online";
  const creditsText = (count) => `${count} ${count === 1 ? "credit" : "credits"}`;

  // ---------------------------------------------------------------- the account button
  function renderButton() {
    const button = $("account-button");
    const waiting = !account.signedIn && account.waiting;
    button.classList.toggle("signed-in", account.signedIn);
    button.classList.toggle("waiting", waiting);

    if (!account.configured) {
      button.textContent = "Account";
      button.title = "Fanan accounts aren't set up in this build yet";
    } else if (account.signedIn) {
      button.textContent = (account.email[0] || "?").toUpperCase();
      button.title = `Signed in as ${account.email}` + (account.credits >= 0 ? ` · ${creditsText(account.credits)}` : "");
    } else if (waiting) {
      button.textContent = "Signing in…";
      button.title = "Finish signing in in your browser";
    } else {
      button.textContent = "Sign in";
      button.title = "Sign in to your Fanan account to use Fanan AI";
    }
  }

  // ---------------------------------------------------------------- the account window
  function renderDialog() {
    const views = {
      missing: !account.configured,
      in: account.configured && account.signedIn,
      waiting: account.configured && !account.signedIn && account.waiting,
      out: account.configured && !account.signedIn && !account.waiting,
    };
    for (const [name, shown] of Object.entries(views)) $(`account-${name}`).hidden = !shown;

    // The connection, in one line, with Try again while it's down.
    $("account-connection").hidden = !account.configured;
    $("account-connection").dataset.status = account.status;
    $("account-connection-text").textContent = account.statusText || "";
    $("account-retry").hidden = account.status !== "noInternet" && account.status !== "serverDown";

    const canSignIn = online() && account.siteAvailable && !account.busy;
    $("account-signin").disabled = !canSignIn;
    $("account-signin").textContent = account.busy ? "Opening…" : "Sign in";
    $("account-again").disabled = !canSignIn;

    $("account-email-shown").textContent = account.email;
    $("account-signout-email").textContent = account.email;
    $("account-credits").textContent = account.credits >= 0 ? String(account.credits) : "–";
    $("account-credits-word").textContent = account.credits === 1 ? "credit" : "credits";
    $("account-buy").hidden = !account.siteAvailable;
    $("account-buy").disabled = !online();
    $("account-computer").textContent = account.computer ? `This computer: ${account.computer}` : "";
    $("account-computer").title = "For Fanan's support, if you ever need it";

    $("account-signout-ask").hidden = !askingSignOut;
    $("account-in-actions").hidden = askingSignOut;
    $("account-signout").disabled = !online() || signingOut;
    $("account-signout-yes").disabled = !online() || signingOut;
    $("account-signout-yes").textContent = signingOut ? "Signing out…" : "Sign out";
    greet();
  }

  function render() {
    renderButton();
    renderDialog();
  }

  function say(message, error) {
    const line = $("account-status");
    line.textContent = error || message || "";
    line.classList.toggle("error", Boolean(error));
  }

  function openAccount() {
    say("");
    askingSignOut = false;
    render();
    $("account-dialog").hidden = false;
    Hub.send("accountCheck");   // the credits as they are now
    // The keyboard goes into the window (so Escape and Enter reach it), even with nothing to type.
    (!$("account-out").hidden ? $("account-email") : $("account-close")).focus();
  }

  function closeAccount() {
    const box = $("account-dialog");
    if (box.contains(document.activeElement)) document.activeElement.blur();
    box.hidden = true;
    askingSignOut = false;
  }

  // ---------------------------------------------------------------- the email box
  // The email last signed in with on this computer: suggested in the empty box while the
  // mouse is over it (a click takes it), completed while it's typed, and greeted.
  const lastEmail = () => (account.lastEmail || "").trim();
  const sameEmail = (a, b) => a.trim().toLowerCase() === b.trim().toLowerCase();

  function canSuggest() {
    return !$("account-out").hidden && $("account-email").value === "" && lastEmail() !== "";
  }

  function showSuggestion(show) {
    const box = $("account-email");
    const suggesting = show && canSuggest();
    box.placeholder = suggesting ? lastEmail() : "you@example.com";
    box.classList.toggle("suggesting", suggesting);
  }

  /** Completes the known email as it's typed: the rest is selected, so typing on replaces it. */
  function complete(event) {
    const box = $("account-email");
    const typed = box.value;
    const known = lastEmail();

    // Only when typing at the end, and never while deleting or composing.
    if (!known || !typed || event.isComposing || (event.inputType || "").startsWith("delete")) return;
    if (box.selectionStart !== typed.length || box.selectionEnd !== typed.length) return;

    if (known.length > typed.length && known.toLowerCase().startsWith(typed.toLowerCase())) {
      box.value = typed + known.slice(typed.length);
      box.setSelectionRange(typed.length, box.value.length);
    }
  }

  /** "Welcome back" once the box holds the email last signed in with. */
  function greet() {
    const known = lastEmail();
    const welcome = Boolean(known) && sameEmail($("account-email").value, known);
    $("account-hint").textContent = welcome ? "Welcome back! Press Enter to sign in." : "";
    $("account-hint").classList.toggle("welcome", welcome);
  }

  // ---------------------------------------------------------------- signing in and out
  function signIn() {
    if ($("account-signin").disabled) return;
    const email = $("account-email").value.trim();
    if (email && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) return say("", "That doesn't look like an email address.");

    say("");
    typedEmail = email;
    account.busy = true;   // until the browser has opened
    render();
    Hub.send("accountSignIn", { email, buy: false });
  }

  function signOut() {
    signingOut = true;
    say("");
    render();
    Hub.send("accountSignOut");
  }

  Hub.on("account", (update) => {
    const before = account;
    account = { ...account, ...update };

    if (signingOut && (!account.signedIn || update.error)) {
      signingOut = false;
      askingSignOut = false;
    }
    if (!account.signedIn) askingSignOut = false;
    if (account.signedIn && !before.signedIn) $("account-email").value = "";

    render();

    const open = !$("account-dialog").hidden;
    if (update.message || update.error) {
      if (open) say(update.message, update.error);
      else Hub.toast(update.error || update.message);
    }

    // Back from the browser: said once, wherever the user is.
    if (account.signedIn && !before.signedIn && (before.waiting || open)) {
      if (open) say(`Signed in as ${account.email}.`);
      Hub.toast(`Signed in as ${account.email}` + (account.credits >= 0 ? ` · ${creditsText(account.credits)}` : ""));
    }

    // Signed out (the button that had the keyboard is gone): typing goes to the email box.
    if (open && !$("account-out").hidden && document.activeElement === document.body) $("account-email").focus();
  });

  // ---------------------------------------------------------------- commands
  Hub.commands.account = openAccount;
  Hub.commands.buyCredits = () => Hub.send("accountBuy");

  Hub.account = { state: () => account, open: openAccount };

  Hub.on("ready", () => {
    render();
    $("account-button").addEventListener("click", openAccount);
    $("account-close").addEventListener("click", closeAccount);
    $("account-retry").addEventListener("click", () => Hub.send("accountCheck"));
    $("account-signin").addEventListener("click", signIn);
    $("account-again").addEventListener("click", () => Hub.send("accountSignIn", { email: typedEmail, buy: false }));
    $("account-cancel").addEventListener("click", () => Hub.send("accountSignInCancel"));
    $("account-buy").addEventListener("click", () => Hub.send("accountBuy"));
    $("account-signout").addEventListener("click", () => {
      askingSignOut = true;
      render();
      $("account-signout-no").focus();
    });
    $("account-signout-yes").addEventListener("click", signOut);
    $("account-signout-no").addEventListener("click", () => {
      askingSignOut = false;
      render();
      $("account-signout").focus();
    });

    const box = $("account-email");
    box.addEventListener("input", (event) => {
      complete(event);
      greet();
      say("");
    });
    box.addEventListener("mouseenter", () => showSuggestion(true));
    box.addEventListener("mouseleave", () => showSuggestion(false));
    box.addEventListener("click", () => {
      if (!canSuggest()) return;
      // A click takes the suggestion: filled in and selected, so typing replaces it.
      box.value = lastEmail();
      box.select();
      showSuggestion(false);
      greet();
    });
    box.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        signIn();
      }
    });

    // While the window is open, keys stay in it: Space clicks its buttons rather than playing.
    for (const type of ["keydown", "keyup"]) {
      $("account-dialog").addEventListener(type, (event) => {
        if (event.key !== "Escape") event.stopPropagation();
      });
    }

    // Escape closes the window wherever the keyboard is (a button that just went busy drops it).
    window.addEventListener("keydown", (event) => {
      if (event.key !== "Escape" || $("account-dialog").hidden) return;
      event.preventDefault();
      event.stopImmediatePropagation();
      closeAccount();
    }, true);
  });
})();
