<!-- D:\workspace\Fanan_web_hub\src\base44_fanan_ai_prompt15.md -->
# Base44 prompt: Stella AI Cloud, version 15 (each job its own model, all of them thinking hard)

Open the **Stella AI Cloud** app on Base44 and paste everything below the line into its AI chat. Nothing changes on Cloudflare: the relay passes the new field along as it is.

Until this is done, Sonify keeps working as now: `sonifyChat` ignores the new field and answers with its one model, and `stellaChat` takes an unknown mode as `chat`, as it already does.

---

Fanan AI does jobs of very different sizes, and until now `sonifyChat` asked one model for all of them. Give it modes, the way `stellaChat` has them, so each job goes to the model that fits it; and make every mode in both functions think hard. Make these changes and nothing else:

1. **`sonifyChat`'s modes.** Add a `MODES` table and pick from it by the request's `mode` field (Sonify sends it). An unknown or missing mode is `agent`:

```ts
const MODES = {
  agent:  { model: 'claude-sonnet-5-5', effort: 'high', priceWeight: 1 },
  deep:   { model: 'claude-opus-5-5',   effort: 'high', priceWeight: 2 },
  guide:  { model: 'claude-haiku-5-5',  effort: 'high', priceWeight: 0.5 },
  device: { model: 'claude-opus-5-5',   effort: 'high', priceWeight: 2 },
};
const DEFAULT_MODE = 'agent';
```

   What each is for, so the names make sense: `agent` is Sonify's everyday requests and its phone remote; `deep` is the big jobs (the user's Deep switch in the AI panel, or a request that has already taken many steps); `guide` answers questions about Sonify from its guide; `device` writes a Sonify device's code.

2. **Its model, effort and price.** The chosen mode gives the request's `model` and its `effort` (send `effort` the same way `stellaChat` sends it), and gives the `priceWeight` the answer is charged with, in place of today's `model.includes('opus') ? 2 : 1`. A `guide` answer is charged with weight 0.5, so it costs about half of what it would have cost; the smallest charge stays 1 credit, as now. A request never names a model itself.

3. **The `SONIFY_AI_MODEL` secret.** It stays, and keeps its job of trying a new model out: when it's set, it overrides every mode's model (the price weight then comes from the model's name, 2 for a name with `opus`, 1 otherwise), and the mode's `effort` still applies. When it isn't set, the modes above decide. Don't create the secret and don't ask me for it.

4. **`stellaChat`: a `pick` mode.** Add a fourth mode, `pick`: `model: 'claude-haiku-5-5'`, `effort: 'high'`, `priceWeight: 0.5`, and the same `maxTokens` and other settings as `chat`. It's for a small judgement from numbers already worked out (Sonify's Loop Editor asks it which of the loops it found is the best one). A request picks it by sending `"mode": "pick"`; everything else picks a mode exactly as it does now.

5. **Thinking hard everywhere.** Set `effort` to `'high'` for every mode in `stellaChat`'s `MODES` as well (`chat`, `build`, `foundation` and the new `pick`). Nothing else about those modes changes: same models, same `maxTokens`, same price weights, same thinking settings.

Make sure that:

- The function names, the entity and field names, the secrets, `stellaChat`'s models and price weights, the web search tool, the relay handling (`relayAction`, `prepare`, `finish`), the credit charging, the wallets, the free credits, the packs, PayPal and every reply format stay exactly as they are.
- `stellaChat` picks its mode in all the same places as now (the relay's `prepare`, the relay's `finish` and the direct path), and `pick` is picked the same way in each.
- Every function still works without a signed-in user (the apps and the relay call them).
- Messages users see never name the AI service or a model.

Test `sonifyChat` when you're done, with a test computer and wallet as in the earlier tests: the token of 64 letters `a` (its `token_hash` is `ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb`), a `StellaAccount` with that hash linked to `fanan-test@example.com`, and a `StellaWallet` for that email with 50 credits. Send the message "Answer in one word: what's the capital of France?" with no tools three times, with `"mode": "agent"`, `"mode": "deep"` and `"mode": "guide"`. Expect `ok: true` and a `reply` of Paris each time. Then delete the test records.

Tell me: each answer's `used` and `reply`, which model each call went to (the AI service's answer says it, in its `model` field), the price weight each was charged with, the lines you changed in each function, and which model and settings every mode in both `MODES` tables uses now.
