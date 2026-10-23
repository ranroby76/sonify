<!-- D:\workspace\Fanan_web_hub\src\base44_fanan_ai_prompt12.md -->
# Base44 prompt: Stella AI Cloud, version 12 (the AI is Fanan AI, for every Fanan app)

Open the **Stella AI Cloud** app on Base44 and paste everything below the line into its AI chat. It only renames the assistant; everything else stays as it is.

Then, on Cloudflare, paste the new `stella-relay.js` from this zip into the relay worker and deploy it: its messages say Fanan AI too.

---

Rename the assistant to **Fanan AI** everywhere in this app. Fanan AI is Fanan's one assistant for all of Fanan's apps (Sonify now, more apps later), so it must never carry one app's name. Make these changes and nothing else:

1. In `stellaChat`, replace the `PERSONA` constant with exactly this:

```ts
const PERSONA = `You are Fanan AI, Fanan's assistant. Fanan makes music software, and you work inside whichever Fanan app sends the request: its guide, below, says which app it is, what you do there and which tools you have.
Act first: when the user asks for something, start right away with sensible defaults, then say briefly what you did and offer one or two follow-ups. Ask a question only when you truly can't proceed without the answer. Keep replies short, and never paste code into a reply.
Don't discuss which AI model, company or service powers you. If asked, say you're Fanan AI, Fanan's assistant, and that you can't share details about the technology behind it. Never claim to be a different specific model.`;
```

2. In `sonifyChat`: if it puts its own persona or assistant name in front of the app's system text, change that the same way (it may use the same `PERSONA`). If it has none, don't add one: the app's own guide names the assistant.

3. Every message users can see from this app's backend functions (`stellaChat`, `sonifyChat`, `stellaAccount`, the voice and remote functions, the credit and PayPal functions): "Stella AI" and "Sonify AI" become "Fanan AI". For example: "Fanan AI couldn't answer that. Please try again.", "Fanan AI is busy right now. Please try again in a moment.", "You're out of Fanan AI credits."

4. The site's pages and any email or receipt text (the /buy page, the sign-in and account pages, the Sonify page): where they name the assistant or its credits, say Fanan AI ("Fanan AI credits"); where they name the account, say "Fanan account". A credit pack's name keeps its words, with Stella AI or Sonify AI in it changed to Fanan AI.

Make sure that:

- The function names (`stellaChat`, `stellaAccount`, `sonifyChat` and the others), the entity and field names, the secrets, `MODES`, the relay handling (`relayAction`), the credit charging, the wallets, the PayPal flow and every reply format stay exactly as they are: the apps call them by these names and read these replies.
- Credits people have, past purchases, and the packs' prices and credit amounts don't change.
- The app's own name and its web address stay as they are.
- Every function still works without a signed-in user (the apps and the relay call them).

When you're done, show me the new `PERSONA`, and list every place the name changed: the function or page, the old text and the new text.
