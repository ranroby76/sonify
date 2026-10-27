<!-- D:\workspace\Fanan_web_hub\src\base44_sonify_ai_prompt.md -->

# Base44 prompt: Stella AI Cloud, Sonify AI

Open the **Stella AI Cloud** app on Base44 and paste everything below the line into its AI chat. It adds Sonify to the same accounts and credits as Stella AI Studio and fixes one thing in `stellaChat`; everything else Stella AI Studio uses stays as it is. Your `ANTHROPIC_API_KEY` secret stays as it is.

---

Update this app. **Sonify**, Fanan's music studio (a DAW), now signs in to the same accounts as Stella AI Studio and spends the same credits. Sonify talks to `stellaAccount` exactly as Stella AI Studio does (its computers say `"app": "Sonify 0.2.0"` in `hello`) and signs in through the same `/link` page. Its AI requests go to a new function, `sonifyChat`. Everything users see from Sonify says "Sonify" by Fanan, and its assistant is "Fanan AI" (never "Sonify AI"); never name the AI service behind it on any page.

**1. New backend function `sonifyChat`**, exactly the code below. Make the same two small platform changes you made to `stellaChat` (the opening import line, and reading the AI service key from the app's secrets). It must work without a signed-in user (Sonify calls it).

The model comes from the mode Sonify sends with the request, through the `MODES` table below, the way `stellaChat` does it. An unknown or missing mode is `agent`. The optional secret `SONIFY_AI_MODEL`, read the same way as the key, overrides every mode's model when it's set (one model for everything, for trying a new one out); don't create it and don't ask me for it.

```ts
import { createClientFromRequest } from 'npm:@base44/sdk';

// Sonify AI: Sonify's requests come here. Sonify (Fanan's music studio) signs in to the same
// Fanan accounts as Stella AI Studio and spends the same credits. This function asks the AI
// service with Fanan's key (which never leaves the server), charges the account's credits, and
// answers in Fanan's own format: nothing names the service behind it.
//
// Sonify sends its own instructions and tool definitions. The tool calls run in Sonify, which
// sends their results back in the next request, until the answer is done.
//
// The model comes from the request's mode. Every mode thinks hard (effort 'high'): the models
// differ, the care doesn't. The SONIFY_AI_MODEL secret, when it's set, overrides them all (for
// trying a new model out for a week or so).
//   agent   Sonify's everyday requests, and the phone remote: fast, and a credit's worth a credit
//   deep    the big jobs (the AI panel's Deep, or a request that has taken many steps): the strongest
//   guide   the Guide's "Ask anything about Sonify": it answers from the guide, so the quickest model
//   device  the Device Studio's: it writes a device's code, so the strongest
const MODES = {
  agent:  { model: 'claude-sonnet-5-5', effort: 'high', priceWeight: 1 },
  deep:   { model: 'claude-opus-5-5',   effort: 'high', priceWeight: 2 },
  guide:  { model: 'claude-haiku-5-5',  effort: 'high', priceWeight: 0.5 },
  device: { model: 'claude-opus-5-5',   effort: 'high', priceWeight: 2 },
};
const DEFAULT_MODE = 'agent';
const MAX_TOKENS = 12000;          // one step's answer at most (thinking included): keeps a step well under the time limit
const MAX_MESSAGES = 200;
const MAX_SYSTEM_CHARS = 60000;
const MAX_TOOLS = 64;
const OUTPUT_WEIGHT = 5;           // an output token costs about five input tokens
const CACHE_WRITE_WEIGHT = 1.25;
const CACHE_READ_WEIGHT = 0.1;     // the same as stellaChat (the service itself charges 0.05 for this model)
const TOKENS_PER_CREDIT = 10000;   // weighted tokens per credit: the same as stellaChat

const PERSONA = `You are Fanan AI, Fanan's assistant, here inside Sonify, a music studio (DAW) by Fanan.
You work in the user's song with Sonify's tools: tracks, instruments and effects, notes and clips, the mix and plugin settings.
Act first: when the user asks for something, do it right away with sensible choices, then say briefly what you did. Ask a question only when you truly can't go on without the answer. Keep replies short.
Don't discuss which AI model, company or service powers you. If asked, say you're Fanan AI, Fanan's assistant, and that you can't share details about the technology behind it. Never claim to be a different specific model.`;

const HEX64 = /^[0-9a-f]{64}$/;
const CACHE = { type: 'ephemeral' };

async function sha256Hex(text: string): Promise<string> {
  const digest = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text));
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, '0')).join('');
}

// Tool-call ids carry the AI service's own prefix; Sonify only ever sees neutral ones.
const toApp = (id: string) => String(id).replace(/^toolu_/, 'call_');
const toService = (id: string) => String(id).replace(/^call_/, 'toolu_');

const isThinking = (block: any) => block?.type === 'thinking' || block?.type === 'redacted_thinking';
const isBlankText = (block: any) => block?.type === 'text' && !String(block.text ?? '').trim();

// One of the user's own requests (not tool results): where a request begins.
function isRequest(message: any) {
  return message?.role === 'user'
    && (!Array.isArray(message.content) || !message.content.some((block: any) => block?.type === 'tool_result'));
}

// Fanan's persona first, then Sonify's own instructions, cached: they're the same every time.
function systemBlocks(given: unknown) {
  const blocks: any[] = [{ type: 'text', text: PERSONA }];
  const list: any[] = Array.isArray(given) ? given : given ? [{ type: 'text', text: String(given) }] : [];
  let room = MAX_SYSTEM_CHARS;

  for (const block of list.slice(0, 4)) {
    const text = String(block?.text ?? '').slice(0, room);
    if (!text.trim()) continue;
    blocks.push({ type: 'text', text });
    room -= text.length;
  }

  blocks[blocks.length - 1].cache_control = CACHE;
  return blocks;
}

function cleanTools(given: unknown) {
  if (!Array.isArray(given)) return [];

  const tools: any[] = given.slice(0, MAX_TOOLS)
    .filter((tool: any) => tool && typeof tool.name === 'string' && tool.input_schema && typeof tool.input_schema === 'object')
    .map((tool: any) => ({ name: tool.name, description: String(tool.description ?? ''), input_schema: tool.input_schema }));

  if (tools.length > 0) tools[tools.length - 1].cache_control = CACHE;
  return tools;
}

// The conversation as the AI service takes it. The request under way keeps its blocks exactly as
// they came (its thinking blocks must go back unchanged with the tool results); earlier requests
// lose their thinking blocks, which the service allows. Tool-call ids go back to the service's own.
function cleanMessages(given: any[]) {
  let messages = given.filter((message: any) => message && (message.role === 'user' || message.role === 'assistant'));

  if (messages.length > MAX_MESSAGES) {
    // Cut only before one of the user's own requests, so every tool call keeps its result.
    const newest = messages.slice(-MAX_MESSAGES);
    const start = newest.findIndex(isRequest);
    messages = start >= 0 ? newest.slice(start) : newest;
  }

  let current = 0;
  messages.forEach((message: any, index: number) => { if (isRequest(message)) current = index; });

  return messages.map((message: any, index: number) => {
    if (!Array.isArray(message.content))
      return { role: message.role, content: String(message.content ?? '') };

    let blocks = message.content.filter((block: any) => block && typeof block === 'object').map((block: any) => {
      const { cache_control, ...rest } = block;   // the cache marks are this function's to place
      return rest.type === 'tool_use' ? { ...rest, id: toService(rest.id) }
        : rest.type === 'tool_result' ? { ...rest, tool_use_id: toService(rest.tool_use_id) }
        : rest;
    });

    if (message.role === 'assistant' && index < current) {
      blocks = blocks.filter((block: any) => !isThinking(block) && !isBlankText(block));
      if (blocks.length === 0) blocks = [{ type: 'text', text: '(No reply.)' }];
    }

    return { role: message.role, content: blocks };
  });
}

Deno.serve(async (req) => {
  try {
    const base44 = createClientFromRequest(req);
    const body = await req.json();
    const token = String(body.token || '');

    if (!HEX64.test(token) || !Array.isArray(body.messages) || body.messages.length === 0)
      return Response.json({ ok: false, error: 'invalid', message: 'That request was malformed.' }, { status: 400 });

    const computers = base44.asServiceRole.entities.StellaAccount;
    const wallets = base44.asServiceRole.entities.StellaWallet;
    const computer = (await computers.filter({ token_hash: await sha256Hex(token) }))[0];

    if (!computer)
      return Response.json({ ok: false, error: 'unknown', message: "Fanan's server doesn't recognise this computer yet. Restart Sonify while online." }, { status: 401 });

    // Sonify AI is for signed-in accounts: it spends the account's credits.
    if (!computer.linked_email)
      return Response.json({ ok: false, error: 'sign_in', message: 'Sign in to use Fanan AI.' }, { status: 401 });

    const wallet = (await wallets.filter({ user_email: computer.linked_email }))[0];
    const balance = wallet ? (wallet.credits || 0) : 0;

    if (!wallet || balance <= 0)
      return Response.json({ ok: false, error: 'no_credits', message: "You're out of credits.", credits: 0 }, { status: 402 });

    const mode = MODES[String(body.mode || '').trim()] ? String(body.mode).trim() : DEFAULT_MODE;
    const settings = MODES[mode];
    const override = String(Deno.env.get('SONIFY_AI_MODEL') || '').trim();
    const model = override || settings.model;
    const priceWeight = override ? (model.includes('opus') ? 2 : 1) : settings.priceWeight;

    const request: Record<string, unknown> = {
      model,
      max_tokens: Math.max(1024, Math.min(MAX_TOKENS, Number(body.max_tokens) || MAX_TOKENS)),
      effort: settings.effort,
      system: systemBlocks(body.system),
      messages: cleanMessages(body.messages),
      cache_control: CACHE,   // the conversation so far is cached as well: each step pays a fraction for what came before
    };

    const tools = cleanTools(body.tools);
    if (tools.length > 0) request.tools = tools;

    const answer = await fetch('https://api.anthropic.com/v1/messages', {
      method: 'POST',
      headers: {
        'x-api-key': Deno.env.get('ANTHROPIC_API_KEY') ?? '',
        'anthropic-version': '2023-06-01',
        'content-type': 'application/json',
      },
      body: JSON.stringify(request),
    });
    const data = await answer.json();

    if (!answer.ok) {
      console.error('AI service error', answer.status, JSON.stringify(data));   // in your logs only
      const busy = answer.status === 429 || answer.status === 529 || answer.status >= 500;
      const tooLong = answer.status === 400 && /too long|context window|too many tokens/i.test(String(data?.error?.message ?? ''));
      return Response.json({
        ok: false,
        error: busy ? 'busy' : tooLong ? 'too_long' : 'failed',
        message: busy ? 'Fanan AI is busy right now. Please try again in a moment.'
               : tooLong ? 'This conversation got too long. Press New chat, then ask again.'
               : "Fanan AI couldn't answer that. Please try again.",
      }, { status: 502 });
    }

    // A declined request isn't charged.
    const refused = data.stop_reason === 'refusal';
    const usage = data.usage || {};
    const weighted = ((usage.input_tokens || 0) + CACHE_WRITE_WEIGHT * (usage.cache_creation_input_tokens || 0)
                    + CACHE_READ_WEIGHT * (usage.cache_read_input_tokens || 0) + OUTPUT_WEIGHT * (usage.output_tokens || 0)) * priceWeight;
    const used = refused ? 0 : Math.max(1, Math.ceil(weighted / TOKENS_PER_CREDIT));
    const inputTokens = usage.input_tokens || 0;
    const outputTokens = usage.output_tokens || 0;

    await computers.update(computer.id, {
      requests: (computer.requests || 0) + 1,
      input_tokens: (computer.input_tokens || 0) + inputTokens,
      output_tokens: (computer.output_tokens || 0) + outputTokens,
      last_seen: new Date().toISOString(),
    });

    // The wallet as it is now: credits bought (or spent in Stella AI Studio) while this step ran are kept.
    const current = (await wallets.filter({ user_email: computer.linked_email }))[0] || wallet;
    const credits = Math.max(0, (current.credits || 0) - used);

    await wallets.update(current.id, {
      credits,
      requests: (current.requests || 0) + 1,
      input_tokens: (current.input_tokens || 0) + inputTokens,
      output_tokens: (current.output_tokens || 0) + outputTokens,
    });

    // What Sonify needs, in Fanan's own format. Thinking blocks go to Sonify exactly as they came:
    // it sends them back with its tool results.
    const content = refused ? [] : (data.content || []).flatMap((block: any) =>
      block.type === 'text' ? (isBlankText(block) ? [] : [{ type: 'text', text: block.text }])
      : block.type === 'tool_use' ? [{ type: 'tool_use', id: toApp(block.id), name: block.name, input: block.input }]
      : isThinking(block) ? [block]
      : []);
    const reply = content.filter((b: any) => b.type === 'text').map((b: any) => b.text).join('\n').trim();

    const stop = data.stop_reason === 'tool_use' ? 'tools'
               : data.stop_reason === 'max_tokens' || data.stop_reason === 'model_context_window_exceeded' ? 'cut'
               : refused ? 'refused'
               : 'done';

    return Response.json({ ok: true, reply, content, stop, used, credits });
  } catch (error) {
    console.error(error);
    return Response.json({ ok: false, error: 'failed', message: "Fanan AI couldn't answer that. Please try again." }, { status: 500 });
  }
});
```

**2. Page `/link`**, when its address has `app=sonify`:
- After linking it says: "You're signed in as {email}. Go back to Sonify; it shows your account within a few seconds."
- With `next=buy`, it goes on to `/buy?app=sonify`.
- Any other text that names Stella AI Studio or Stella AI says Sonify or Fanan AI instead, and the footer line says "Fanan AI requests are processed by a third-party AI service provider."
- Signing in on the way keeps the whole address (`t`, `email`, `next` and `app`), as it does now for `t` and `next`.

**3. Page `/buy`**, when its address has `app=sonify`: the same changes of name, and after paying it says: "Done. Your credits appear in Sonify within a few seconds, on every computer you're signed in on. They work in Stella AI Studio too." The packs, prices and PayPal stay exactly as they are.

Without `app=sonify`, the `/link` and `/buy` pages stay exactly as they are now.

**4. Function `stellaLinkComputer`:** its two error messages that say "Press Sign in in Stella AI Studio again." now say "Press Sign in in the app again." Nothing else in it changes.

**5. Page `/account` and the admin page:** each computer shows its app (its `app` field: "Stella AI Studio ..." or "Sonify ..."), so they can be told apart.

**6. One fix in `stellaChat`**, now that both apps spend the same wallets: it reads the balance before the AI call and writes back `balance - used` afterwards, so credits bought (or spent in Sonify) while a step runs are lost. Read the wallet (or, for a computer that isn't signed in, the computer's own record) again right before writing, and subtract `used` from that fresh value, as `sonifyChat` does. Nothing else in it changes.

**7. Nothing else changes:** `stellaAccount`, signing in, the free credits, the packs, the prices and PayPal all stay exactly as they are.

**8. Test `sonifyChat` when you're done**, with this token (64 times the letter a):
`aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa`
Its `token_hash` is `ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb`.
1. With no `StellaAccount` for that hash: expect 401 with error `unknown`.
2. Make a test `StellaAccount` with that `token_hash` and an empty `linked_email`: expect 401 with error `sign_in`.
3. Set its `linked_email` to `sonify-test@example.com`, and make a `StellaWallet` for that email with 0 credits: expect 402 with error `no_credits`.
4. Give that wallet 50 credits and send `{"token": "<the token>", "messages": [{"role": "user", "content": "Say hello in five words."}]}`: expect 200 with `ok: true`, `stop: "done"`, a short `reply`, `used` of at least 1, and the wallet's credits lowered by `used`.

Then delete the test `StellaAccount` and `StellaWallet`, and tell me the four answers and which model the function used.
