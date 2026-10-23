<!-- D:\workspace\Fanan_web_hub\src\base44_fanan_ai_prompt13.md -->
# Base44 prompt: Stella AI Cloud, version 13 (Fanan AI researches on the web)

Two parts, in either order:

1. Open the **Stella AI Cloud** app on Base44 and paste everything below the line into its AI chat.
2. On Cloudflare, paste the new `stella-relay.js` from this zip into the relay worker (Workers & Pages > the relay > Edit code) and deploy it: it now passes the web searches through.

---

Give Fanan AI web search. Fanan AI (the `stellaChat` function, which Sonify's Plugins Creator Studio calls through the Cloudflare relay) may now look things up on the web while it works: the AI service runs the searches itself, inside a step. This app only adds the tool, passes its blocks along unchanged, and charges for the searches. Make these changes and nothing else:

1. **The search tool.** Wherever `stellaChat` builds the request to the AI service (the relay's `prepare`, and the direct path when it answers without the relay), add this tool at the start of `tools`, for every mode:

```ts
const WEB_SEARCH = { type: 'web_search_20250305', name: 'web_search', max_uses: 3 };
```

Add it after the app's own tools are cleaned: it has no `input_schema`, so it must not go through the cleaning that drops tools without one. The cache mark stays on the last tool in the list, as now. If the app sent no tools, the list is just this one.

2. **The conversation passes search blocks through.** In the messages the app sends, blocks of type `server_tool_use` and `web_search_tool_result`, and the `citations` array on `text` blocks, go to the AI service exactly as they came: no field removed or changed, and their ids are not renamed (only `toolu_` / `call_` ids are renamed, as now). The service refuses a request whose search results were changed.

3. **The answer passes them through.** Where the AI service's `content` becomes the app's `content` (the relay's `finish`, and the direct path), keep `server_tool_use` and `web_search_tool_result` blocks exactly as they came, and keep a `text` block's `citations` when it has them. `reply` stays the joined text only.

4. **A paused answer.** When `stop_reason` is `pause_turn` (the service paused a long search), `stop` is `"paused"` (today it falls through to `"done"`). It's charged like any other answer; the app sends it back and the step carries on.

5. **Charging searches.** `usage.server_tool_use.web_search_requests` is how many searches the answer made (0 or missing: none). Add 5000 weighted tokens per search, outside the model's price weight: `weighted += 5000 * searches`, before `weighted` becomes credits. A refused answer still costs nothing.

6. **Model price weights, in `stellaChat` and `sonifyChat`.** The model's price weight (today 2 for a model whose name has `opus`, 1 otherwise) becomes: 5 for a name with `fable` or `mythos`, 2 for `opus`, 1 otherwise. Those models cost 2.5 times as much as Opus to run: without this, switching to one would charge far too little.

Make sure that:

- The function names, the entity and field names, the secrets, `MODES` and each mode's model and settings, the relay handling (`relayAction`, `prepare`, `finish`), the wallets, the free credits, the packs, PayPal and every reply format stay exactly as they are.
- Every function still works without a signed-in user (the apps and the relay call them).
- Messages users see never name the AI service.

Test `stellaChat` when you're done, on the direct path (no `relayAction`), with a test computer and wallet as in the earlier tests: the token of 64 letters `a` (its `token_hash` is `ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb`), a `StellaAccount` with that hash linked to `fanan-test@example.com`, and a `StellaWallet` for that email with 50 credits. Send `"mode": "chat"` with the message "Look up on the web the year the Roland Juno-106 came out, and answer in one sentence." with no tools. Expect `ok: true`, a `content` with a `server_tool_use` block and a `web_search_tool_result` block, a `reply` that says 1984, and `used` that includes the search. Then delete the test records.

Tell me: the test's answer (its `stop`, `used` and `reply`, and the block types in `content`), the lines you changed in each function, and which model and settings each mode in `MODES` uses now.
