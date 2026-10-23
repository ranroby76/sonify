<!-- D:\workspace\Fanan_web_hub\src\base44_sonify_remote_prompt.md -->

# Base44 prompt: Stella AI Cloud, Sonify Remote

Open the **Stella AI Cloud** app on Base44 and paste into its AI chat:
- **Part 1** is done: it's in the app and tested. It's kept here for the record; don't paste it again.
- **Part 2**, the page, typing only: paste it now. It needs no new secret and adds no fees. On the phone, talking still works through the keyboard's own mic key.
- **Part 3**, a talk button on the page itself: later, only if you want it. It uses the paid speech service: paste `base44_sonify_listen_prompt.md` first (it adds the `OPENAI_API_KEY` secret), then Part 3.

Until Sonify has its **Phone remote** switch, the page just waits for Sonify.

---

## Part 1: done (already in the app, don't paste it again)

Update this app. Add **Sonify Remote**: a page for phones, `/sonify` (Part 2 builds it), where a musician in the recording room talks to Sonify AI, the AI inside Sonify (Fanan's music studio, a DAW) on their studio computer. They tap a button and speak, or type; their words go to the Sonify signed in to the same Fanan account; Sonify AI does it in the song, and its reply shows on the phone. Sonify talks to the server through a new function, `sonifyRemote`; the page through another, `sonifyRemotePage`. Users never see which AI or speech service is behind it, on any page.

**1. New entity `SonifyRemote`**: one record per studio computer that turned Sonify's Phone remote on. Fields:
- `token_hash` (text): the computer, as in `StellaAccount`
- `user_email` (text): the account the computer is signed in to
- `name` (text): the computer's name, as Sonify gives it
- `enabled` (boolean)
- `seen_at` (number): when Sonify last checked in, in milliseconds since 1970
- `state` (long text): Sonify AI's conversation, as JSON text that Sonify sends
- `state_at` (number)
- `request_id` (text), `request_kind` (text), `request_text` (text), `request_spoken` (boolean), `request_at` (number): what the phone asked for, waiting for Sonify

No user reads or changes it directly: only these two functions do, as the service role. Give it the same access rules as `StellaAccount`.

**2. New backend function `sonifyRemote`** (Sonify calls it), exactly the code below. Make the same platform change you made to `sonifyChat` (the opening import line). It must work without a signed-in user.

```ts
import { createClientFromRequest } from 'npm:@base44/sdk';

// Sonify Remote, the studio computer's side. With "Phone remote" on, Sonify (signed in to a
// Fanan account) opens its remote here, then checks in every second or two: it posts its
// Sonify AI conversation for the phone to show, and picks up what the phone asked for. The
// page /sonify, signed in to the same account on a phone, does the other half through
// sonifyRemotePage. Only these two functions touch SonifyRemote records.
//
// Actions (all with Sonify's token, as for sonifyChat):
//   open  { name }   the switch went on: the remote is ready, nothing waiting
//   poll  { state? } still here; the conversation when it changed (Sonify's own JSON, kept as
//                    text). Answers { request: { id, kind: "ask" | "stop", text, spoken } | null }:
//                    a request is handed over once, and dropped if it waited too long
//   close            the switch went off (or Sonify is closing)
const HEX64 = /^[0-9a-f]{64}$/;
const MAX_STATE = 60000;          // characters of conversation the phone is shown
const SEEN_EVERY_MS = 5000;       // "still here" is written at most this often
const REQUEST_LIFE_MS = 30000;    // a request Sonify didn't pick up by then is dropped, not done late

const NOTHING_WAITING = { request_id: '', request_kind: '', request_text: '', request_spoken: false, request_at: 0 };

async function sha256Hex(text: string): Promise<string> {
  const digest = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text));
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, '0')).join('');
}

const fail = (status: number, error: string, message: string) => Response.json({ ok: false, error, message }, { status });

Deno.serve(async (req) => {
  try {
    const base44 = createClientFromRequest(req);
    const body = await req.json();
    const token = String(body.token || '');
    const action = String(body.action || '');

    if (!HEX64.test(token) || !['open', 'poll', 'close'].includes(action))
      return fail(400, 'invalid', 'That request was malformed.');

    const computers = base44.asServiceRole.entities.StellaAccount;
    const remotes = base44.asServiceRole.entities.SonifyRemote;
    const tokenHash = await sha256Hex(token);
    const computer = (await computers.filter({ token_hash: tokenHash }))[0];

    if (!computer)
      return fail(401, 'unknown', "Fanan's server doesn't recognise this computer yet. Restart Sonify while online.");

    const remote = (await remotes.filter({ token_hash: tokenHash }))[0];
    const now = Date.now();

    if (action === 'close') {
      if (remote) await remotes.update(remote.id, { enabled: false, state: '', ...NOTHING_WAITING });
      return Response.json({ ok: true });
    }

    // The phone signs in to the same account, so the computer must be signed in.
    if (!computer.linked_email) {
      if (remote && remote.enabled) await remotes.update(remote.id, { enabled: false, state: '', ...NOTHING_WAITING });
      return fail(401, 'sign_in', 'Sign in to use Sonify Remote.');
    }

    if (action === 'open') {
      const fields = {
        user_email: computer.linked_email,
        name: String(body.name ?? '').trim().slice(0, 60) || 'Sonify',
        enabled: true,
        seen_at: now,
        state: '',
        state_at: now,
        ...NOTHING_WAITING,
      };

      if (remote) await remotes.update(remote.id, fields);
      else await remotes.create({ token_hash: tokenHash, ...fields });

      return Response.json({ ok: true });
    }

    // poll
    if (!remote || !remote.enabled)
      return fail(409, 'closed', 'Sonify Remote is off.');   // Sonify opens it again

    const fields: Record<string, unknown> = {};
    let request = null;

    if (remote.user_email !== computer.linked_email) {
      // The computer has signed in to another account since: the remote follows it, and what
      // the old account's phone asked for, or was shown, goes.
      Object.assign(fields, { user_email: computer.linked_email, state: '', state_at: now, ...NOTHING_WAITING });
    } else if (remote.request_id) {
      if (now - (remote.request_at || 0) <= REQUEST_LIFE_MS)
        request = { id: remote.request_id, kind: remote.request_kind === 'stop' ? 'stop' : 'ask', text: remote.request_text || '',
                    spoken: remote.request_spoken === true };

      Object.assign(fields, NOTHING_WAITING);   // handed over once, or too old to do now
    }

    if (typeof body.state === 'string') {
      fields.state = body.state.slice(0, MAX_STATE);
      fields.state_at = now;
    }

    if (now - (remote.seen_at || 0) >= SEEN_EVERY_MS)
      fields.seen_at = now;

    if (Object.keys(fields).length > 0)
      await remotes.update(remote.id, fields);

    return Response.json({ ok: true, request });
  } catch (error) {
    console.error(error);
    return fail(500, 'failed', "Fanan's server couldn't do that. Please try again.");
  }
});
```

**3. New backend function `sonifyRemotePage`** (the page calls it, with the user signed in), exactly the code below. Make the same platform changes you made to `sonifyListen` (the opening import line, and reading the `OPENAI_API_KEY` secret, which is already set; `SONIFY_LISTEN_MODEL` stays optional, as there).

```ts
import { createClientFromRequest } from 'npm:@base44/sdk';

// Sonify Remote, the phone's side: the page /sonify, signed in to the user's Fanan account,
// talks with the Sonify signed in to the same account on the studio computer (sonifyRemote is
// Sonify's side). The phone sends what to ask Sonify AI, and shows the conversation Sonify
// posts. It also turns what the user says into words, as sonifyListen does for Sonify (the
// same speech service, the same daily limit, nothing charged, nothing kept).
//
// Every answer is HTTP 200: { ok: true, ... }, or { ok: false, error, message } with a message
// the page shows as it is.
//   list                          the user's Sonify computers with Phone remote on, answering now
//   state  { remote, since? }     { name, online, pending, stateAt, state } (state: Sonify's conversation, or
//                                 null). With since = the stateAt last answered and nothing new: unchanged: true, no state
//   send   { remote, kind, text, spoken }   kind "ask" (with text) or "stop" (stop Sonify AI). Answers { id }
//   listen { audio }              a short 16-bit WAV (base64) of what the user said. Answers { text }
const ONLINE_MS = 15000;              // a Sonify heard from within this long is there
const REQUEST_LIFE_MS = 30000;        // as in sonifyRemote: a request not picked up by then is gone
const MAX_TEXT = 2000;
const DEFAULT_MODEL = 'gpt-transcribe';   // the SONIFY_LISTEN_MODEL secret, when it's set, as in sonifyListen
const MIN_BYTES = 1000;
const MAX_BYTES = 2_000_000;
const DAILY_SECONDS = 3 * 60 * 60;    // shared with Sonify's voice control: one limit per account and day (UTC)

const answer = (fields: Record<string, unknown>) => Response.json({ ok: true, ...fields });
const refuse = (error: string, message: string) => Response.json({ ok: false, error, message });

function fromBase64(text: string): Uint8Array | null {
  try {
    const binary = atob(text);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
    return bytes;
  } catch {
    return null;
  }
}

// A plain WAV file (PCM), and how many seconds of sound it holds.
function wavSeconds(bytes: Uint8Array): number {
  if (bytes.length < 44) return -1;
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const tag = (at: number) => String.fromCharCode(bytes[at], bytes[at + 1], bytes[at + 2], bytes[at + 3]);
  if (tag(0) !== 'RIFF' || tag(8) !== 'WAVE' || tag(12) !== 'fmt ' || view.getUint16(20, true) !== 1) return -1;
  const bytesPerSecond = view.getUint32(28, true);
  return bytesPerSecond > 0 ? (bytes.length - 44) / bytesPerSecond : -1;
}

function parseState(text: unknown) {
  if (typeof text !== 'string' || !text) return null;
  try {
    return JSON.parse(text);
  } catch {
    return null;
  }
}

Deno.serve(async (req) => {
  try {
    const base44 = createClientFromRequest(req);

    let user: any = null;
    try {
      user = await base44.auth.me();
    } catch {
      user = null;
    }

    if (!user?.email)
      return refuse('sign_in', 'Sign in to use Sonify Remote.');

    const email = String(user.email);
    const body = await req.json().catch(() => ({}));
    const action = String(body?.action || '');
    const remotes = base44.asServiceRole.entities.SonifyRemote;
    const now = Date.now();
    const isOnline = (r: any) => r.enabled === true && now - (r.seen_at || 0) < ONLINE_MS;
    const isWaiting = (r: any) => Boolean(r.request_id) && now - (r.request_at || 0) <= REQUEST_LIFE_MS;

    if (action === 'list') {
      const mine = await remotes.filter({ user_email: email });
      return answer({ remotes: mine.filter(isOnline).map((r: any) => ({ id: r.id, name: r.name || 'Sonify' })) });
    }

    if (action === 'state' || action === 'send') {
      // Only the user's own: a remote belongs to the account its computer is signed in to.
      const remote = (await remotes.filter({ user_email: email })).find((r: any) => r.id === String(body?.remote || ''));

      if (!remote)
        return refuse('not_found', "That Sonify isn't signed in to your account.");

      if (action === 'state') {
        const stateAt = remote.state_at || 0;
        const unchanged = stateAt > 0 && Number(body?.since) === stateAt;   // the phone has it already
        return answer({ name: remote.name || 'Sonify', online: isOnline(remote), pending: isWaiting(remote), stateAt,
                        ...(unchanged ? { unchanged: true } : { state: parseState(remote.state) }) });
      }

      if (!isOnline(remote))
        return refuse('offline', "Sonify isn't answering. Check that it's open and online, with Phone remote on.");

      const kind = body?.kind === 'stop' ? 'stop' : 'ask';
      const text = String(body?.text ?? '').trim().slice(0, MAX_TEXT);

      if (kind === 'ask' && !text)
        return refuse('invalid', 'Say or type something first.');

      if (kind === 'ask' && isWaiting(remote))
        return refuse('busy', "Sonify hasn't picked up the last request yet. Try again in a moment.");

      const id = crypto.randomUUID();
      await remotes.update(remote.id, { request_id: id, request_kind: kind, request_text: kind === 'ask' ? text : '',
                                        request_spoken: kind === 'ask' && body?.spoken === true, request_at: now });
      return answer({ id });
    }

    if (action === 'listen') {
      const audio = typeof body?.audio === 'string' ? fromBase64(body.audio) : null;
      const seconds = audio ? wavSeconds(audio) : -1;

      if (!audio || audio.length < MIN_BYTES || audio.length > MAX_BYTES || seconds <= 0)
        return refuse('invalid', "That recording didn't work. Please try again.");

      const wallets = base44.asServiceRole.entities.StellaWallet;
      const wallet = (await wallets.filter({ user_email: email }))[0];

      if (!wallet || (wallet.credits || 0) <= 0)
        return refuse('no_credits', "You're out of credits.");

      const today = new Date().toISOString().slice(0, 10);
      const heardToday = wallet.listen_day === today ? (wallet.listen_seconds || 0) : 0;

      if (heardToday + seconds > DAILY_SECONDS)
        return refuse('listen_limit', "Talking to Sonify has reached today's limit for this account. Type instead, or try again tomorrow.");

      const form = new FormData();
      form.append('file', new Blob([audio.buffer as ArrayBuffer], { type: 'audio/wav' }), 'speech.wav');   // the whole buffer: it holds just the file
      form.append('model', String(Deno.env.get('SONIFY_LISTEN_MODEL') || DEFAULT_MODEL).trim());

      const heard = await fetch('https://api.openai.com/v1/audio/transcriptions', {
        method: 'POST',
        headers: { Authorization: `Bearer ${Deno.env.get('OPENAI_API_KEY') ?? ''}` },
        body: form,
      });
      const data = await heard.json().catch(() => ({}));

      if (!heard.ok) {
        console.error('Speech service error', heard.status, JSON.stringify(data?.error ?? data));   // in your logs only
        const busy = heard.status === 429 || heard.status >= 500;
        return refuse(busy ? 'busy' : 'failed', busy ? 'Too busy to listen right now. Try again in a moment.' : "Couldn't make out the words. Please try again.");
      }

      // Counted once it's heard, on the wallet as it is now.
      const current = (await wallets.filter({ user_email: email }))[0] || wallet;
      const before = current.listen_day === today ? (current.listen_seconds || 0) : 0;
      await wallets.update(current.id, { listen_day: today, listen_seconds: Math.round((before + seconds) * 10) / 10 });

      return answer({ text: String(data?.text ?? '').trim() });
    }

    return refuse('invalid', 'That request was malformed.');
  } catch (error) {
    console.error(error);
    return refuse('failed', "Fanan's server couldn't do that. Please try again.");
  }
});
```

**4. Nothing else changes:** `sonifyChat`, `sonifyListen`, `stellaChat`, `stellaAccount`, the pages, signing in, the credits and PayPal stay exactly as they are.

**5. Test both functions when you're done**, with this token (64 times the letter a):
`aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa`
Its `token_hash` is `ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb`.
1. `sonifyRemote` with `{"token": "<the token>", "action": "open", "name": "Test PC"}` and no `StellaAccount` for that hash: expect 401 with error `unknown`.
2. Make a test `StellaAccount` with that `token_hash` and `linked_email` set to `sonify-test@example.com`, then send the same `open`: expect 200 with `ok: true`, and a `SonifyRemote` record for that hash with `enabled: true` and `name: "Test PC"`.
3. `sonifyRemote` with `{"token": "<the token>", "action": "poll", "state": "{\"busy\":false,\"entries\":[]}"}`: expect 200 with `ok: true` and `request: null`, and that text in the record's `state`.
4. Set the record's `request_id` to `test-1`, `request_kind` to `ask`, `request_text` to `hello` and `request_at` to the time now (milliseconds), then `poll` again: expect `request` to be `{"id": "test-1", "kind": "ask", "text": "hello", "spoken": false}`, and the record's `request_id` empty again.
5. `sonifyRemotePage` with `{"action": "list"}`, as yourself: expect `ok: true` and an empty `remotes` list (the test computer belongs to the test account, not to you).
6. `sonifyRemotePage` with `{"action": "send", "remote": "<the test record's id>", "kind": "ask", "text": "hi"}`, as yourself: expect `ok: false` with error `not_found`.
7. `sonifyRemote` with `{"token": "<the token>", "action": "close"}`: expect `ok: true` and the record's `enabled` false.

Then delete the test `SonifyRemote` and `StellaAccount` records, and tell me the seven answers.

---

## Part 2: paste this now (typing only)

Build the **Sonify Remote** page, `/sonify`, for phones first. It uses the `sonifyRemotePage` function from Part 1, and one helper file. Typing only: no microphone button on the page, and it never calls `listen`.

**1. One helper file, exactly as below:** `components/sonify/replyText.js` (Sonify AI's replies, shown as Sonify shows them). Import it in the page.

```js
// Sonify Remote: Sonify AI's replies shown as Sonify shows them. Its marks never show:
// **bold** and *italic* become fonts, `code` plain words, "# " lines headings, "- " lines bullets,
// fenced code is left out, and ==a question or a task for the user== shows in yellow (when a
// reply marks nothing, its questions are found by their "?").
//
//   replyLines(text) -> [{ kind: '' | 'heading' | 'bullet', runs: [{ text, bold, italic, ask }] }]
//
// Each line is a block (a bullet gets a dot before it, a heading is bold); each run is a span.
// Give each line dir="auto", so Hebrew reads right to left.

const FENCE = '`'.repeat(3);   // a fenced code block's mark

export function replyLines(text) {
  const lines = [];
  let inCode = false;

  for (const line of String(text || '').split(/\r?\n/)) {
    if (line.trimStart().startsWith(FENCE)) inCode = !inCode;
    else if (!inCode) lines.push(line);
  }

  while (lines.length && !lines[lines.length - 1].trim()) lines.pop();

  const marked = String(text || '').includes('==');

  return lines.map((original) => {
    let line = original;
    const trimmed = line.trimStart();
    let kind = '';

    if (trimmed === '---' || trimmed === '***' || trimmed === '___') line = '';
    else if (trimmed.startsWith('#')) { line = trimmed.replace(/^#+\s*/, ''); kind = 'heading'; }
    else if (/^([-*]|•) /.test(trimmed)) { line = trimmed.slice(2); kind = 'bullet'; }
    else if (trimmed.startsWith('> ')) line = trimmed.slice(2);

    if (!marked) line = markQuestions(line);

    return { kind, runs: parseLine(line, kind === 'heading') };
  });
}

// A line's inline marks, each only when it's closed on the same line.
function parseLine(line, bold) {
  const runs = [];
  let italic = false, code = false, ask = false;
  let current = { text: '', bold, italic, ask };

  const flush = () => {
    if (current.text) runs.push(current);
    current = { text: '', bold, italic, ask };
  };
  const closedLater = (mark, from) => line.indexOf(mark, from) >= 0;

  for (let i = 0; i < line.length;) {
    const rest = line.slice(i);

    if (!code && rest.startsWith('**') && (bold || closedLater('**', i + 2))) {
      flush(); bold = !bold; current.bold = bold; i += 2;
    } else if (!code && rest.startsWith('==') && (ask || closedLater('==', i + 2))) {
      flush(); ask = !ask; current.ask = ask; i += 2;
    } else if (line[i] === '`' && (code || closedLater('`', i + 1))) {
      flush(); code = !code; i += 1;
    } else if (!code && line[i] === '*' && (italic || (i + 1 < line.length && line[i + 1] !== ' ' && closedLater('*', i + 1)))) {
      flush(); italic = !italic; current.italic = italic; i += 1;
    } else {
      current.text += line[i]; i += 1;
    }
  }

  flush();
  return runs;
}

// When a reply marks nothing for the user, its questions are found by their "?".
function markQuestions(line) {
  let result = '', sentence = '';

  for (let i = 0; i < line.length; i++) {
    sentence += line[i];
    const end = line[i] === '.' || line[i] === '!' || line[i] === '?';

    if (end && (i + 1 === line.length || line[i + 1] === ' ')) {
      const trimmed = sentence.trim();
      result += line[i] === '?' && trimmed ? sentence.slice(0, sentence.length - trimmed.length) + '==' + trimmed + '==' : sentence;
      sentence = '';
    }
  }

  return result + sentence;
}
```

**2. The page `/sonify`:**
- **On its own:** none of the site's header, menu, footer or links, and no link to it anywhere else on the site. Its own look: dark (background #1b1c20, panels #2b2c30, text #eceded, quiet text #8e939a, green #25984d, the user's bubbles #275533, questions yellow #f2cf3c, problems #f0a7a9), "Sonify Remote" at the top with "by Fanan" under it, big touch targets, and it fills the phone's screen with the input bar at the bottom (above the keyboard while typing).
- **Signed out:** the site's sign-in, then back to `/sonify`.
- **Finding Sonify:** call `sonifyRemotePage` with `{"action": "list"}` every 3 seconds until there's one. None: "Turn on **Phone remote** in Sonify's AI panel, on your studio computer. It must be signed in as {the user's email}.", with this line under it in quiet text: "Your requests to Sonify AI are processed by third-party AI service providers." One: use it. Several: let the user pick by name, and remember the choice on this phone.
- **Connected:** call `{"action": "state", "remote": <its id>, "since": <the stateAt of the last answer>}` every 1.5 seconds while the page is in view (not while the phone is locked or the tab is hidden). An answer with `unchanged: true` has nothing new: keep what's shown. Show its `name` with a green dot. When `online` stays false for 10 seconds, go back to finding Sonify.
- **What `state` holds** (Sonify's conversation, or null until Sonify sends it): `{"busy": false, "credits": 120, "taken": "<the id of the last request Sonify picked up>", "entries": [...]}`, where each entry is `{"role": "user", "text": "...", "spoken": true}`, `{"role": "assistant", "text": "...", "steps": ["Added a reverb", ...]}` or `{"role": "notice", "text": "..."}`.
- **The conversation:** the entries in order, scrolled to the newest. `user` entries are right-aligned green bubbles, with a small mic icon when `spoken`. `assistant` entries are left-aligned panel bubbles: first their `steps`, each a small line with a green ✓, then their `text` drawn with `replyLines` from replyText.js (a block per line with `dir="auto"`, bullets with a dot before them, headings bold, and each run a span: bold, italic, and `ask` in the yellow). `notice` entries are small text in the problem colour. With no entries yet: "Tell Sonify AI what you want in your song., with this line under it in quiet text: "To talk instead of typing, tap the mic on your phone's keyboard." While `busy`: "Sonify AI is working…" under the last entry, and a **Stop** button that sends `{"action": "send", "remote": <id>, "kind": "stop"}`.
- **Credits:** `credits` (when it's there) as "N credits", in quiet text at the top.
- **The input bar:** a text box ("Ask Sonify AI…") and a big Send button. Send, or Enter, sends `{"action": "send", "remote": <id>, "kind": "ask", "text": <the box's text>}` and empties the box. While `busy`, or while `pending` is true, the box and Send are disabled.
- **A request on its way:** show it at once as a faded user bubble. It goes when Sonify has it: when `state.taken` equals the `id` that `send` answered (the real one is then in the entries). If that doesn't happen within 10 seconds: "Sonify didn't get that." with a Try again button that sends it again.
- **Problems:** every answer with `ok: false` has a `message`: show it as it is, in the problem colour, just above the input bar, until the next call that works. A call that gets no answer at all: "No connection. Trying again…", and keep trying.

**3. Nothing else changes.**

**4. Check it:** open `/sonify` on a phone-sized screen, signed in: it shows the "Turn on **Phone remote**…" message, with none of the site's header or menu, and no microphone button.

---

## Part 3: later, only for a talk button (paste `base44_sonify_listen_prompt.md` first)

Add tap-to-talk to the **Sonify Remote** page, `/sonify`. It uses the `listen` action of `sonifyRemotePage` (it reads the `OPENAI_API_KEY` secret, which is now set) and one more helper file.

**1. A new helper file, exactly as below:** `components/sonify/recorder.js` (tap-to-talk: records the user and makes the WAV that `listen` takes). Import it in the page.

```js
// Sonify Remote: tap-to-talk on the phone. Records what the user says and makes it a short WAV
// (16 kHz, 16-bit, mono, base64) for sonifyRemotePage's "listen", as Sonify's own voice control
// sends it.
//
//   const recording = await startRecording({ maxSeconds: 30, onLevel, onLimit });
//   ... later: const { audio, seconds } = await recording.stop();   (or recording.cancel())
//
// Call startRecording straight from the tap, with nothing awaited before it: phones only let a
// tap start sound. The first time, the browser asks the user to allow the microphone. It throws
// an Error whose message can be shown as it is. onLevel(0..1) follows the voice, to animate the
// button; onLimit() is called once when maxSeconds is reached (then call stop()).

const RATE = 16000;

export async function startRecording({ maxSeconds = 30, onLevel, onLimit } = {}) {
  const AudioContextClass = window.AudioContext || window.webkitAudioContext;

  if (!AudioContextClass || !navigator.mediaDevices || !navigator.mediaDevices.getUserMedia)
    throw new Error("This browser can't record. Type instead.");

  // Made and started inside the tap, before anything is awaited.
  const context = new AudioContextClass();
  const resumed = context.resume();
  let stream;

  try {
    stream = await navigator.mediaDevices.getUserMedia({
      audio: { channelCount: 1, echoCancellation: true, noiseSuppression: true, autoGainControl: true },
    });
  } catch (error) {
    context.close();
    throw new Error(error && error.name === 'NotAllowedError'
      ? 'Allow the microphone for this site, then tap again.'
      : "The microphone couldn't start. Type instead.");
  }

  await resumed;

  const source = context.createMediaStreamSource(stream);
  const processor = context.createScriptProcessor(4096, 1, 1);
  const chunks = [];
  const limit = Math.round(maxSeconds * context.sampleRate);
  let total = 0, finished = false, limitTold = false;

  processor.onaudioprocess = (event) => {
    if (finished) return;

    const input = event.inputBuffer.getChannelData(0);
    chunks.push(new Float32Array(input));
    total += input.length;

    if (onLevel) {
      let sum = 0;
      for (let i = 0; i < input.length; i++) sum += input[i] * input[i];
      onLevel(Math.min(1, Math.sqrt(sum / input.length) * 4));
    }

    if (total >= limit && !limitTold) {
      limitTold = true;
      if (onLimit) onLimit();
    }
  };

  source.connect(processor);
  processor.connect(context.destination);   // some browsers only run it when it's connected; it plays nothing

  const finish = () => {
    finished = true;
    processor.onaudioprocess = null;
    processor.disconnect();
    source.disconnect();
    stream.getTracks().forEach((track) => track.stop());
    context.close();
  };

  return {
    async stop() {
      if (finished) throw new Error('That recording has already ended.');

      const inputRate = context.sampleRate;
      finish();

      const recorded = new Float32Array(Math.min(total, limit));
      let at = 0;
      for (const chunk of chunks) {
        const part = chunk.subarray(0, Math.min(chunk.length, recorded.length - at));
        recorded.set(part, at);
        at += part.length;
        if (at >= recorded.length) break;
      }

      const samples = resample(recorded, inputRate);
      return { audio: toBase64(toWav(samples)), seconds: samples.length / RATE };
    },

    cancel() {
      if (!finished) finish();
    },
  };
}

// To 16 kHz: below its Nyquist first (a fourth-order low-pass at 6.5 kHz), then linear interpolation.
function resample(samples, rate) {
  if (rate === RATE) return samples;

  const filtered = rate > RATE ? lowPass(lowPass(samples, rate, 6500, 0.5412), rate, 6500, 1.3066) : samples;
  const step = rate / RATE;
  const out = new Float32Array(filtered.length > 0 ? Math.floor((filtered.length - 1) / step) + 1 : 0);

  for (let i = 0; i < out.length; i++) {
    const position = i * step, index = Math.floor(position), fraction = position - index;
    const a = filtered[index], b = index + 1 < filtered.length ? filtered[index + 1] : a;
    out[i] = a + (b - a) * fraction;
  }

  return out;
}

function lowPass(samples, rate, frequency, q) {
  const w0 = (2 * Math.PI * frequency) / rate, cos = Math.cos(w0), alpha = Math.sin(w0) / (2 * q), a0 = 1 + alpha;
  const b0 = (1 - cos) / 2 / a0, b1 = (1 - cos) / a0, b2 = b0, a1 = (-2 * cos) / a0, a2 = (1 - alpha) / a0;
  const out = new Float32Array(samples.length);
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;

  for (let i = 0; i < samples.length; i++) {
    const x = samples[i];
    const y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x; y2 = y1; y1 = y;
    out[i] = y;
  }

  return out;
}

// 16-bit WAV, its loudest sound brought up near full scale (by 30 dB at most).
function toWav(samples) {
  let peak = 0;
  for (let i = 0; i < samples.length; i++) peak = Math.max(peak, Math.abs(samples[i]));
  const gain = peak > 0 ? Math.min(0.7 / peak, 31.6) : 1;

  const bytes = new Uint8Array(44 + samples.length * 2);
  const view = new DataView(bytes.buffer);
  const text = (at, value) => { for (let i = 0; i < 4; i++) bytes[at + i] = value.charCodeAt(i); };

  text(0, 'RIFF');
  view.setUint32(4, 36 + samples.length * 2, true);
  text(8, 'WAVE');
  text(12, 'fmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);          // PCM
  view.setUint16(22, 1, true);          // mono
  view.setUint32(24, RATE, true);
  view.setUint32(28, RATE * 2, true);   // bytes a second
  view.setUint16(32, 2, true);          // bytes a sample
  view.setUint16(34, 16, true);
  text(36, 'data');
  view.setUint32(40, samples.length * 2, true);

  for (let i = 0; i < samples.length; i++)
    view.setInt16(44 + i * 2, Math.max(-32767, Math.min(32767, Math.round(samples[i] * gain * 32767))), true);

  return bytes;
}

function toBase64(bytes) {
  let binary = '';
  for (let i = 0; i < bytes.length; i += 0x8000)
    binary += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
  return btoa(binary);
}
```

**2. Changes to `/sonify`:**
- **The input bar:** a big round mic button before the text box. While `busy`, or while `pending` is true, it's disabled with the box and Send.
- **The mic, tap to talk:** a tap calls `startRecording({ maxSeconds: 30, onLevel, onLimit })` straight from the tap, with nothing awaited before it. While it records, the button is red with a ring that grows with `onLevel`, a timer beside it, and "Tap to send". The next tap (or `onLimit`) calls `stop()`. Under half a second (`seconds` < 0.5): "Too short. Tap, talk, then tap again." Otherwise show "Listening…", send `{"action": "listen", "audio": <audio>}`, and send the words it answers as `{"action": "send", "remote": <id>, "kind": "ask", "text": <the words>, "spoken": true}`. No words: "Didn't catch that. Try again." `startRecording` throws an Error with a message to show as it is. Leaving the page, or switching to another Sonify, calls `cancel()`.
- **Finding Sonify:** the quiet line becomes "What you say here, and your requests to Sonify AI, are processed by third-party AI service providers."
- **No entries yet:** the quiet line under "Tell Sonify AI what you want in your song." becomes "Tap the mic and talk, or type."

**3. Nothing else changes.**

**4. Check it:** on a phone, signed in, with Sonify's Phone remote on: tap the mic, allow the microphone, say "add a reverb to the vocals", then tap again. The words show as your bubble with a mic icon, and Sonify AI does it.
