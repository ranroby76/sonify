<!-- D:\workspace\Fanan_web_hub\src\base44_sonify_listen_prompt.md -->

# Base44 prompt: Stella AI Cloud, Sonify voice control

Open the **Stella AI Cloud** app on Base44 and paste everything below the line into its AI chat. It adds one function for Sonify's voice control and changes nothing Stella AI Studio uses. It needs one new secret, `OPENAI_API_KEY`: when the chat asks for it, give it an API key from your OpenAI account.

---

Update this app. **Sonify**, Fanan's music studio (a DAW), now has voice control: a musician in the recording room talks to Sonify through their mic ("Sonify, record, over"). Sonify finds each phrase in the mic's sound and sends it to a new function, `sonifyListen`, which turns it into words with the speech service and answers with them. Sonify does the rest itself. Users never see which speech service is behind it, on any page.

**1. New backend function `sonifyListen`**, exactly the code below. Make the same two small platform changes you made to `sonifyChat` (the opening import line, and reading the service's key from the app's secrets); this one reads a new secret, `OPENAI_API_KEY`: ask me for it. It must work without a signed-in user (Sonify calls it).

The model is one setting, an optional secret named `SONIFY_LISTEN_MODEL`, read the same way as the key. Don't create it and don't ask me for it: when it isn't set, the function uses `gpt-transcribe`.

```ts
import { createClientFromRequest } from 'npm:@base44/sdk';

// Sonify's voice control: a musician in the recording room talks to Sonify through their mic
// ("Sonify, record, over"). Sonify finds each phrase in the mic's sound and sends it here, a
// short 16 kHz WAV; this function sends it to the speech service with Fanan's key (which never
// leaves the server) and answers with the words. Sonify itself listens for "Sonify ... over"
// and does the rest. This function keeps nothing: not the sound, not the words.
//
// Nothing is charged, but it's for signed-in accounts with credits, and each account has a
// daily limit of listening, so a mic left on all day can't run up the bill.
//
// The model is one setting: the SONIFY_LISTEN_MODEL secret when it's set, the default below when
// it isn't.
const DEFAULT_MODEL = 'gpt-transcribe';
const MIN_BYTES = 1000;
const MAX_BYTES = 2_000_000;           // about a minute of Sonify's sound; it sends 25 seconds at most
const DAILY_SECONDS = 3 * 60 * 60;     // of listening, per account and day (UTC)
const HEX64 = /^[0-9a-f]{64}$/;

async function sha256Hex(text: string): Promise<string> {
  const digest = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text));
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, '0')).join('');
}

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

const fail = (status: number, error: string, message: string) => Response.json({ ok: false, error, message }, { status });

Deno.serve(async (req) => {
  try {
    const base44 = createClientFromRequest(req);
    const body = await req.json();
    const token = String(body.token || '');
    const audio = typeof body.audio === 'string' ? fromBase64(body.audio) : null;
    const seconds = audio ? wavSeconds(audio) : -1;

    if (!HEX64.test(token) || !audio || audio.length < MIN_BYTES || audio.length > MAX_BYTES || seconds <= 0)
      return fail(400, 'invalid', 'That recording was malformed.');

    const computers = base44.asServiceRole.entities.StellaAccount;
    const wallets = base44.asServiceRole.entities.StellaWallet;
    const computer = (await computers.filter({ token_hash: await sha256Hex(token) }))[0];

    if (!computer)
      return fail(401, 'unknown', "Fanan's server doesn't recognise this computer yet. Restart Sonify while online.");

    if (!computer.linked_email)
      return fail(401, 'sign_in', 'Sign in to use voice control.');

    const wallet = (await wallets.filter({ user_email: computer.linked_email }))[0];

    if (!wallet || (wallet.credits || 0) <= 0)
      return fail(402, 'no_credits', "You're out of credits.");

    const today = new Date().toISOString().slice(0, 10);
    const heardToday = wallet.listen_day === today ? (wallet.listen_seconds || 0) : 0;

    if (heardToday + seconds > DAILY_SECONDS)
      return fail(429, 'listen_limit', "Voice control has reached today's limit for this account. It works again tomorrow.");

    const form = new FormData();
    form.append('file', new Blob([audio.buffer as ArrayBuffer], { type: 'audio/wav' }), 'speech.wav');   // the whole buffer: it holds just the file
    form.append('model', String(Deno.env.get('SONIFY_LISTEN_MODEL') || DEFAULT_MODEL).trim());

    const answer = await fetch('https://api.openai.com/v1/audio/transcriptions', {
      method: 'POST',
      headers: { Authorization: `Bearer ${Deno.env.get('OPENAI_API_KEY') ?? ''}` },
      body: form,
    });
    const data = await answer.json().catch(() => ({}));

    if (!answer.ok) {
      console.error('Speech service error', answer.status, JSON.stringify(data?.error ?? data));   // in your logs only
      const busy = answer.status === 429 || answer.status >= 500;
      return fail(502, busy ? 'busy' : 'failed',
                  busy ? 'Voice control is busy right now. Try again in a moment.' : "Voice control couldn't understand that right now.");
    }

    // Counted once it's heard. The wallet as it is now: what Sonify AI or Stella AI Studio wrote meanwhile stays.
    const current = (await wallets.filter({ user_email: computer.linked_email }))[0] || wallet;
    const before = current.listen_day === today ? (current.listen_seconds || 0) : 0;
    await wallets.update(current.id, { listen_day: today, listen_seconds: Math.round((before + seconds) * 10) / 10 });

    return Response.json({ ok: true, text: String(data?.text ?? '').trim() });
  } catch (error) {
    console.error(error);
    return fail(500, 'failed', "Voice control couldn't understand that right now.");
  }
});
```

**2. Entity `StellaWallet`:** add two fields, `listen_day` (text, a date such as 2026-10-08) and `listen_seconds` (number, default 0). They hold each account's voice control for the day (it has a daily limit); nothing else uses them, and no page shows them.

**3. Page `/link`**, when its address has `app=sonify`: its footer line now says "Sonify AI requests and voice control are processed by third-party AI service providers."

**4. Nothing else changes:** `sonifyChat`, `stellaChat`, `stellaAccount`, signing in, the credits, the packs, the prices and PayPal all stay exactly as they are.

**5. Test `sonifyListen` when you're done**, with this token (64 times the letter a):
`aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa`
Its `token_hash` is `ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb`.
For the sound, make a one-second WAV of silence (16 kHz, 16-bit, mono: the 44-byte header and then 32,000 zero bytes) and send it base64-encoded, as `{"token": "<the token>", "format": "wav", "audio": "<the WAV in base64>"}`.
1. With no `StellaAccount` for that hash: expect 401 with error `unknown`.
2. Make a test `StellaAccount` with that `token_hash` and an empty `linked_email`: expect 401 with error `sign_in`.
3. Set its `linked_email` to `sonify-test@example.com`, and make a `StellaWallet` for that email with 0 credits: expect 402 with error `no_credits`.
4. Give that wallet 50 credits: expect 200 with `ok: true` and a `text` (empty or nearly, for silence), the wallet's `listen_day` set to today's date (UTC) and its `listen_seconds` to 1, and its credits still 50.
5. Set its `listen_seconds` to 10800: expect 429 with error `listen_limit`.

Then delete the test `StellaAccount` and `StellaWallet`, and tell me the five answers and which model the function used.
