<!-- D:\workspace\Fanan_web_hub\src\base44_fanan_ai_prompt14.md -->
# Base44 prompt: Stella AI Cloud, version 14 (a new plugin's foundation is built by the strongest model)

Open the **Stella AI Cloud** app on Base44 and paste everything below the line into its AI chat. Nothing changes on Cloudflare: the relay passes the new field along as it is.

Until this is done, Sonify keeps working as now: the app ignores the new field and builds with its usual builder.

---

Give Fanan AI's builder a foundation mode. When Sonify's Plugins Creator Studio starts a new plugin, every builder step until the plugin's first successful build (the foundation the whole plugin grows from) goes to the strongest model; after that the builder carries on with its usual model. The studio says which by sending `"foundation": true` beside `"mode": "build"`. Make these changes and nothing else:

1. **The mode.** In `stellaChat`'s `MODES`, add a third mode, `foundation`: a copy of `build` (the same `maxTokens`, `effort`, thinking and every other setting) with `model: 'claude-fable-5-1'` and `priceWeight: 5`. That model costs 2.5 times as much as Opus to run.

2. **Picking it.** Where `stellaChat` picks the mode from the request: `"mode": "build"` with `"foundation": true` picks `foundation`; `"mode": "build"` without it picks `build`, as now; anything else picks `chat`, as now. The same rule everywhere the mode is picked: the relay's `prepare` (the request it returns), the relay's `finish` (the price weight it charges with: the same request comes back, so it picks the same mode) and the direct path (no `relayAction`). A request never names a model itself.

3. **Its price.** A `foundation` answer is charged with price weight 5. If the price weight already comes from the model's name (5 for a name with `fable` or `mythos`, 2 for `opus`, 1 otherwise), it gives 5 by itself; check that it does.

4. **When it fails.** When the AI service fails for a `foundation` request (an error status, the model unavailable or overloaded), answer exactly as for any other failure (`ok: false` with its `error` and message, nothing charged), and never try another model here: the studio sends the step again to the usual builder by itself. An answer the model declined (`stop_reason` `refusal`) is `stop: "refused"` and costs nothing, as now.

Make sure that:

- The function names, the entity and field names, the secrets, the `chat` and `build` modes and their settings, the web search tool, the relay handling (`relayAction`, `prepare`, `finish`), the wallets, the free credits, the packs, PayPal and every reply format stay exactly as they are.
- Every function still works without a signed-in user (the apps and the relay call them).
- Messages users see never name the AI service or a model.

Test `stellaChat` when you're done, on the direct path (no `relayAction`), with a test computer and wallet as in the earlier tests: the token of 64 letters `a` (its `token_hash` is `ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb`), a `StellaAccount` with that hash linked to `fanan-test@example.com`, and a `StellaWallet` for that email with 50 credits. Send the message "Answer in one word: what's the capital of France?" with no tools twice: once with `"mode": "build", "foundation": true`, once with `"mode": "build"` alone. Expect `ok: true` and a `reply` of Paris both times (an answer this short costs the least either way). Then delete the test records.

Tell me: both answers' `used` and `reply`, which model each call went to (the AI service's answer says it, in its `model` field), the price weight each was charged with, the lines you changed, and which model and settings each mode in `MODES` uses now.
