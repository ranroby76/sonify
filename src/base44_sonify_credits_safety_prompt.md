<!-- D:\workspace\Fanan_web_hub\src\base44_sonify_credits_safety_prompt.md -->
# Base44 prompt: Stella AI Cloud, credits that can't be overdrawn, and honest voice seconds

Paste everything below the line into the app's AI chat. Publish when it's done. No new secrets.

---

Update this app. Two holes are closed: **`sonifyChat` can be overdrawn** (a wallet with 1 credit can pay for a request of any size, and several requests sent at once all run before any is charged), and **the voice limit can be dodged** (a recording's length is read from a number its sender writes in it). Change only what's described here.

**1. `sonifyChat`: what a request may carry.**
- Refuse a request body bigger than 400 KB (as JSON text): status 413, `{ ok: false, error: 'too_long', message: 'This conversation got too long. Press New chat, then ask again.' }`.
- In `cleanMessages`, keep only these content block types: `text`, `image` (only with `source.type` `'base64'`, `media_type` `image/png`, `image/jpeg`, `image/gif` or `image/webp`, and at most 5 MB of data), `tool_use`, `tool_result` (its own content: text and such images only), `thinking` and `redacted_thinking`. Any other block (`document`, an image from a URL, a server tool's block) is left out of what goes to the AI service.

**2. `sonifyChat`: credits reserved before the call.**
- Before calling the AI service, work out the most the request can cost: `reserve = Math.ceil(((bodyChars / 3) + OUTPUT_WEIGHT * maxTokens) * priceWeight / TOKENS_PER_CREDIT)`, where `bodyChars` is the length of `JSON.stringify` of the system, messages and tools being sent, and `maxTokens` the `max_tokens` being sent. (Cached input costs less than this, so the reserve is never too small.)
- When the wallet's credits are below `reserve`: status 402, `{ ok: false, error: 'no_credits', message: "This request needs up to N credits, and you have M. Buy credits, or press New chat for a shorter conversation.", credits: M }` (N and M the numbers). The smallest request still needs at least 1 credit, as now.
- At most **2 requests at a time per wallet** (Sonify's Guide can translate a page while a question is answered). Add a field `in_flight` (long text: a JSON array of `{ id, until }`) to `StellaWallet`. Before the call: drop entries whose `until` has passed; if 2 remain, answer status 429, `{ ok: false, error: 'busy', message: 'Fanan AI is still answering your last requests. Please wait a moment.' }`; else add `{ id: <a new random id>, until: now + 180000 }`, save, wait 150 ms, read the wallet again, and if the saved list no longer holds this id, or holds more than 2 live entries, take this id out and answer the same 429 (another request took the place at the same moment).
- Then take the reserve off the wallet's credits **before** the call (`credits - reserve`, saved). After the call, give back `reserve - used` (never more than the reserve), from the wallet as it is then, and take this request's id out of `in_flight`. When the call fails, or the answer is refused, give back the whole reserve. Whatever happens (an error thrown too), the id leaves `in_flight` and an unspent reserve goes back: use `try`/`finally`.
- `used` and the reply format stay as they are: the answer's `credits` is the wallet's balance after the reserve was settled.

**3. The voice seconds (`sonifyListen`, and the `listen` action of `sonifyRemote`).** Replace `wavSeconds` in both with this, which reads the recording's real format and the size of its sound, not the rate its sender claims:

```ts
/** A WAV recording's length in seconds: plain PCM, 16-bit, mono, 8000 to 48000 Hz, its byte
    rate exactly what its format makes it, and the seconds from the size of its sound data.
    -1 when it isn't one. */
function wavSeconds(bytes: Uint8Array): number {
  if (bytes.length < 44) return -1;
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const tag = (at: number) => String.fromCharCode(bytes[at], bytes[at + 1], bytes[at + 2], bytes[at + 3]);
  if (tag(0) !== 'RIFF' || tag(8) !== 'WAVE') return -1;

  let rate = 0, dataBytes = -1;

  for (let at = 12; at + 8 <= bytes.length;) {
    const id = tag(at), size = view.getUint32(at + 4, true), body = at + 8;

    if (id === 'fmt ') {
      if (size < 16 || body + 16 > bytes.length) return -1;
      const format = view.getUint16(body, true), channels = view.getUint16(body + 2, true);
      rate = view.getUint32(body + 4, true);
      const byteRate = view.getUint32(body + 8, true), bits = view.getUint16(body + 14, true);
      if (format !== 1 || channels !== 1 || bits !== 16 || rate < 8000 || rate > 48000 || byteRate !== rate * 2) return -1;
    } else if (id === 'data') {
      dataBytes = Math.min(size, bytes.length - body);
      break;
    }

    at = body + size + (size & 1);
  }

  return rate > 0 && dataBytes > 0 ? dataBytes / (rate * 2) : -1;
}
```

And the daily limit is **reserved before the speech service is called**: add the recording's seconds to the wallet's `listen_seconds` (with `listen_day` today) and save it before the call; if the call fails, take them back off. A recording that would pass the limit is refused before anything is saved, as now.

**4. Nothing else changes:** the modes, models, effort and price weights, the charge formula (`used`), the free credits, the packs, PayPal, `stellaChat`, `stellaAccount`, the Store, the pages and every other reply format stay exactly as they are.

**5. Test when you're done**, on the published version, with the test computer (token of 64 `a`, linked to `fanan-test@example.com`) and its wallet:
1. Wallet at 1 credit. `sonifyChat` with `mode: 'deep'`, `max_tokens: 12000` and a short message: expect `no_credits` with "needs up to" in the message, and the wallet still at 1. Wallet at 200: the same request answers `ok`, and the wallet ends at `200 - used` (check the reserve came back).
2. Wallet at 200. Send three requests at the same moment (`Promise.all`): expect at most two `ok` and at least one `busy`; afterwards `in_flight` is empty and the wallet is `200 -` the sum of the `used` of the ones that answered.
3. A request whose message holds a `document` block and an image from a URL: expect `ok`, and (in your logs) neither block reached the AI service.
4. `sonifyListen` with a WAV whose header says 16 kHz mono 16-bit but whose byte rate field says 999999999: expect `invalid`. A real 2-second 16 kHz mono 16-bit WAV: expect `ok`, and `listen_seconds` up by 2.
5. Delete the test records.

Tell me each test's result and the lines you changed in each function.
