# Base44 prompt: the Hub's AI (server side)

Paste everything below the line into Base44's AI chat, in the **Fanan Hub Cloud app** (the same app as the accounts). Then add your Anthropic API key as described after it.

---

Add an AI backend to this app. The Fanan Web Hub desktop app sends AI requests to a backend function; the function calls Anthropic's Claude API with a secret key that never leaves the server, and counts each user's monthly allowance.

**1. User field:** add `ai_plan` to users: text, `free` or `pro`, default `free`. Only admins can change it (users can't edit their own plan).

**2. Entity `AiUsage`** (one record per user per month):
- `user_email`: text
- `month`: text (`YYYY-MM`)
- `requests`: number
- `last_request_id`: text
- `calls_this_request`: number
- `input_tokens`: number
- `output_tokens`: number

Security: no access for regular users at all (the backend function uses the service role). Admins can read everything.

**3. Secret:** a secret named `ANTHROPIC_API_KEY`, used only by the function below.

**4. Backend function `aiChat`**, exactly this code:

```ts
import { createClientFromRequest } from 'npm:@base44/sdk';

// The Fanan Web Hub's AI: forwards the Hub's request to Claude as the signed-in user,
// within their plan's monthly allowance. The API key never leaves this server.
const MODEL = 'claude-sonnet-5';
const ALLOWANCE: Record<string, number> = { free: 30, pro: 1000 };
const MAX_CALLS_PER_REQUEST = 40;
const MAX_TOKENS = 8192;

Deno.serve(async (req) => {
  try {
    const base44 = createClientFromRequest(req);
    const user = await base44.auth.me();
    if (!user) return Response.json({ error: 'Sign in to use the AI.' }, { status: 401 });

    const body = await req.json();
    const requestId = String(body.request_id || '');
    if (!requestId || !Array.isArray(body.messages))
      return Response.json({ error: 'That request was malformed.' }, { status: 400 });

    const plan = user.ai_plan === 'pro' ? 'pro' : 'free';
    const allowance = ALLOWANCE[plan];
    const month = new Date().toISOString().slice(0, 7);
    const found = await base44.asServiceRole.entities.AiUsage.filter({ user_email: user.email, month });
    const usage = found[0];
    const isNewRequest = !usage || usage.last_request_id !== requestId;
    const used = usage ? (usage.requests || 0) : 0;

    if (isNewRequest && used >= allowance)
      return Response.json({
        error: `You've used this month's ${allowance} AI requests.` + (plan === 'free' ? ' Upgrade for more.' : ''),
        fanan: { used, allowance, plan },
      }, { status: 429 });

    if (!isNewRequest && (usage.calls_this_request || 0) >= MAX_CALLS_PER_REQUEST)
      return Response.json({ error: 'This request took too many steps.', fanan: { used, allowance, plan } }, { status: 429 });

    const answer = await fetch('https://api.anthropic.com/v1/messages', {
      method: 'POST',
      headers: {
        'x-api-key': Deno.env.get('ANTHROPIC_API_KEY') ?? '',
        'anthropic-version': '2023-06-01',
        'content-type': 'application/json',
      },
      body: JSON.stringify({
        model: MODEL,
        max_tokens: Math.min(Number(body.max_tokens) || 4096, MAX_TOKENS),
        system: body.system,
        tools: body.tools,
        messages: body.messages,
      }),
    });
    const data = await answer.json();

    if (!answer.ok)
      return Response.json({ error: data?.error?.message || 'The AI service failed.', fanan: { used, allowance, plan } }, { status: 502 });

    // A new request counts once; every call adds its tokens.
    const fields = {
      user_email: user.email,
      month,
      requests: used + (isNewRequest ? 1 : 0),
      last_request_id: requestId,
      calls_this_request: isNewRequest ? 1 : (usage.calls_this_request || 0) + 1,
      input_tokens: (usage?.input_tokens || 0) + (data.usage?.input_tokens || 0)
        + (data.usage?.cache_read_input_tokens || 0) + (data.usage?.cache_creation_input_tokens || 0),
      output_tokens: (usage?.output_tokens || 0) + (data.usage?.output_tokens || 0),
    };

    if (usage) await base44.asServiceRole.entities.AiUsage.update(usage.id, fields);
    else await base44.asServiceRole.entities.AiUsage.create(fields);

    return Response.json({ ...data, fanan: { used: fields.requests, allowance, plan } });
  } catch (error) {
    return Response.json({ error: String((error as Error)?.message || error) }, { status: 500 });
  }
});
```

**5. Admin page "AI usage"** (admins only): this month's AiUsage records, newest first, with each user's plan, requests, and tokens; and a way to set a user's `ai_plan` by hand.

---

**Afterwards:**
- **Your Anthropic key:** create one at the Claude Console (platform.claude.com) under your Fanan organization, add a payment method there, then paste the key into the app's secret `ANTHROPIC_API_KEY`. Anthropic bills Fanan for all usage.
- **Backend functions need Base44's Builder plan or higher.**
- **To test before payments exist:** set your own account's `ai_plan` to `pro` on the admin page.
