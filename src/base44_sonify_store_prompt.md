<!-- D:\workspace\Fanan_web_hub\src\base44_sonify_store_prompt.md -->
# Base44 prompt: Stella AI Cloud, Sonify's Store (the notice board)

Before you paste, in this order:
1. **Cloudflare:** the files worker stays as it is (`relay\sonify-store-files.js`). If it only accepts the file names `package.zip` and `picture.png`, let it accept `logo.png` too, with the same checks.
2. **Base44 > Stella AI Cloud > Settings > Secrets:** keep `STORE_FILES_SECRET` and `STORE_FILES_URL`. `STORE_LICENCE_KEY` isn't used any more: delete it. The PayPal secrets stay (the credit packs use them).
3. **Paste** everything below the line into the app's AI chat. Publish when it's done.

---

Update this app. **Sonify's store becomes a notice board**, still called the Store. Sonify users (Sonify is Fanan's music studio, a DAW) post **ads** for the plugins and MIDI FX they make in Sonify's Plugins Creator Studio: what it is, its story and features, a YouTube video, a picture and their logo, the formats it comes in, a price or terms in their own words (shown as written), and a link to **their own website**, where they sell it and register their buyers themselves. **Nothing is sold through Fanan any more:** no PayPal orders, no purchases, no licences, no signing keys. A poster may also offer the Sonify version of their plugin for instant download (their choice, per ad): Sonify downloads it from the ad. The files (each ad's picture, logo and optional package) stay in Cloudflare R2 behind the worker; this app keeps the ads and signs the addresses Sonify sends the files to.

**1. Remove the old store:** the entities `StoreProduct` and `StorePurchase`, and the pages `/store-paid` and `/store-cancelled`. If either entity holds records other than test records, tell me how many before you delete it. Remove nothing else.

**2. New entity `StoreAd`**: one record per ad. Fields:
- `ad` (text): the ad's id, made here, never changes
- `seller_key` (text): the poster's own id for what it advertises in Sonify (a project's or a MIDI FX's id), so a post from the same project finds its ad again
- `seller_email` (text): the poster's Fanan account
- `kind` (text): `instrument`, `effect`, `widget` or `midifx`
- `title` (text), `tagline` (text), `story` (long text), `features` (long text: a JSON array of short texts)
- `section` (text): one of its kind's sections (the list is in the code below)
- `maker` (text): the maker's name as Sonify shows it
- `video` (text): a YouTube link, or empty
- `website` (text): the poster's website (http or https)
- `formats` (text: a JSON array of `VST3`, `CLAP`, `AU`, `Sonify`)
- `price_text` (text): the poster's own words on price or terms, shown as written
- `offers_download` (boolean): the Sonify version can be downloaded from the ad
- `picture_key` (text), `logo_key` (text): the files in R2 (`logo_key` empty: no logo)
- `package_key` (text), `package_size` (number), `package_sha256` (text), `package_version` (number): the Sonify version's package, when one was sent
- `version` (number): counts the ad's posts (0: never posted)
- `pending_version` (number), `pending_at` (number): a post under way
- `hidden` (boolean): taken off the board by its poster (or by Fanan)
- `created_at` (number), `updated_at` (number): milliseconds since 1970

No user reads or changes it directly: only `sonifyStore` does, as the service role. Give it the same access rules as `StellaAccount`.

**3. Replace the backend function `sonifyStore`** with exactly the code below (Sonify calls it). Keep the same platform change you made to `sonifyRemote` (the opening import line). It must work without a signed-in user. If the entity `filter` takes its sort and its limit differently from `filter(query, '-updated_at', 1000)`, adapt that one call and keep its meaning (every visible ad, newest first); if deleting a record isn't `entities.StoreAd.delete(id)`, adapt that one call. Change nothing else, and tell me what you adapted.

```ts
import { createClientFromRequest } from 'npm:@base44/sdk';

// Sonify's Store: a notice board. Sonify users post ads for the plugins and MIDI FX they make in
// Sonify (Fanan's music studio, a DAW), with a link to their own website, where they sell them
// and register their buyers. Nothing is sold here: no payments, no licences. An ad may offer the
// Sonify version for instant download. The files (each post's picture and logo, and the optional
// package) are kept in Cloudflare R2 behind the sonify-store-files worker; this function signs
// the addresses Sonify sends them to.
//
// Actions (with Sonify's token, as for sonifyChat):
//   list     { kind?, section?, query?, page? }      anyone: { ads: [...], page, pages, total }
//   get      { ad }                                  anyone: { ad: {...} } (a hidden one: its poster only)
//   mine                                             signed in: { ads: [...] }, hidden ones too
//   begin    { key, kind, title, ad?, package }      signed in: the ad's id (made the first time, by
//            key), the post's number and where to send its files (a package only when package is
//            true). Answers { ad, version, uploads: { picture, logo, package? } }
//   publish  { ad, version, title, tagline, story, section, features, video, website, formats,
//              priceText, maker, offersDownload, picture, logo, package? }   the files are there:
//            the ad is on the board. package { size, sha256 }: a new package was sent; without it,
//            an ad that offers the download keeps the package it had
//   hide     { ad }  /  unhide { ad }                the poster takes their ad off the board, or back
//   delete   { ad }                                  the poster deletes their ad for good
const HEX64 = /^[0-9a-f]{64}$/;
const AD_ID = /^[A-Za-z0-9._-]{1,80}$/;
const KINDS = ['instrument', 'effect', 'widget', 'midifx'];
const SECTIONS: Record<string, string[]> = {
  instrument: ['Synths', 'Keys & Pianos', 'Pads & Atmospheres', 'Bass', 'Drums & Percussion', 'Leads & Plucks', 'Orchestral & Acoustic', 'Experimental'],
  effect: ['Reverb & Space', 'Delay & Echo', 'Distortion & Saturation', 'Dynamics', 'EQ & Filters', 'Modulation', 'Pitch & Vocal', 'Creative FX', 'Mixing Utilities'],
  midifx: ['Arpeggiators', 'Chords & Harmony', 'Rhythm & Patterns', 'Generators', 'MIDI Utilities'],
  widget: ['Visualizers & Meters', 'Tools', 'Games & Fun'],
};
const FORMATS = ['VST3', 'CLAP', 'AU', 'Sonify'];
const MAX_TITLE = 80, MAX_TAGLINE = 140, MAX_STORY = 6000, MAX_FEATURES = 10, MAX_FEATURE = 120, MAX_MAKER = 60;
const MAX_WEBSITE = 300, MAX_PRICE_TEXT = 120, MAX_VIDEO = 300;
const MAX_PACKAGE = 64 * 1024 * 1024, MAX_PICTURE = 8 * 1024 * 1024, MAX_LOGO = 2 * 1024 * 1024;
const MAX_ADS_PER_SELLER = 200;
const UPLOAD_LIFE_MS = 30 * 60 * 1000;           // a signed file address works this long
const PAGE_SIZE = 40;

const fail = (status: number, error: string, message: string) => Response.json({ ok: false, error, message }, { status });
const answer = (fields: Record<string, unknown>) => Response.json({ ok: true, ...fields });

const text = (value: unknown, max: number) => String(value ?? '').trim().slice(0, max).trim();

function secret(name: string): string {
  return String(Deno.env.get(name) || '').trim();
}

async function sha256Hex(value: string): Promise<string> {
  const digest = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(value));
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, '0')).join('');
}

async function hmacHex(key: string, value: string): Promise<string> {
  const k = await crypto.subtle.importKey('raw', new TextEncoder().encode(key), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  const signature = await crypto.subtle.sign('HMAC', k, new TextEncoder().encode(value));
  return Array.from(new Uint8Array(signature), (b) => b.toString(16).padStart(2, '0')).join('');
}

function newAdId(): string {
  const letters = 'abcdefghijkmnpqrstuvwxyz23456789';
  const bytes = crypto.getRandomValues(new Uint8Array(12));
  return 'ad-' + Array.from(bytes, (b) => letters[b % letters.length]).join('');
}

function parseList(value: unknown): string[] {
  try {
    const list = JSON.parse(String(value || '[]'));
    return Array.isArray(list) ? list.map(String) : [];
  } catch {
    return [];
  }
}

/** A website's address: http or https, a real host name, nothing odd in it. */
function website(value: unknown): string {
  const link = text(value, MAX_WEBSITE);
  if (!link || /[\s"'<>\\@`]/.test(link)) return '';
  try {
    const url = new URL(link);
    const host = url.hostname.toLowerCase();
    if ((url.protocol !== 'https:' && url.protocol !== 'http:') || !/^[a-z0-9.-]+$/.test(host) || !host.includes('.')
        || host.startsWith('.') || host.endsWith('.') || host.includes('..')) return '';
    return link;
  } catch {
    return '';
  }
}

/** A YouTube video's link, made plain (watch?v=<id>); '' when it isn't one. */
function youtube(value: unknown): string {
  const link = text(value, MAX_VIDEO);
  if (!link) return '';
  if (/^[A-Za-z0-9_-]{11}$/.test(link)) return `https://www.youtube.com/watch?v=${link}`;
  const found = link.match(/^(?:https?:\/\/)?(?:www\.|m\.)?(?:youtube\.com\/(?:watch\?(?:.*&)?v=|shorts\/|embed\/|live\/|v\/)|youtube-nocookie\.com\/embed\/|youtu\.be\/)([A-Za-z0-9_-]{11})(?:[?&#/].*)?$/);
  return found ? `https://www.youtube.com/watch?v=${found[1]}` : '';
}

//==============================================================================
// The store's files (the sonify-store-files worker).
function filesUrl(): string {
  return secret('STORE_FILES_URL').replace(/\/+$/, '');
}

function fileKey(ad: string, version: number, name: 'package.zip' | 'picture.png' | 'logo.png'): string {
  return `products/${ad}/v${version}/${name}`;
}

function publicUrl(key: string): string {
  return key ? `${filesUrl()}/files/${key}` : '';
}

async function uploadUrl(key: string, type: string, max: number): Promise<string> {
  const exp = Date.now() + UPLOAD_LIFE_MS;
  const sig = await hmacHex(secret('STORE_FILES_SECRET'), `PUT\n${key}\n${exp}\n${max}\n${type}`);
  return `${filesUrl()}/upload/${key}?exp=${exp}&max=${max}&type=${encodeURIComponent(type)}&sig=${sig}`;
}

/** A file as the worker keeps it: its size and SHA-256, or null when it isn't there. */
async function fileInfo(key: string): Promise<{ size: number; sha256: string } | null> {
  const response = await fetch(publicUrl(key), { method: 'HEAD' });
  if (response.status === 404) return null;
  if (!response.ok) throw new Error(`files HEAD ${response.status}`);
  return { size: Number(response.headers.get('content-length') || 0), sha256: String(response.headers.get('x-content-sha256') || '') };
}

//==============================================================================
function publicAd(a: any, mine: boolean) {
  const offers = a.offers_download === true && !!a.package_key;
  return {
    ad: a.ad, kind: a.kind, title: a.title, tagline: a.tagline || '', story: a.story || '', features: parseList(a.features),
    section: a.section || '', maker: a.maker || '', video: a.video || '', website: a.website || '', formats: parseList(a.formats),
    priceText: a.price_text || '', picture: publicUrl(a.picture_key), logo: publicUrl(a.logo_key || ''),
    offersDownload: offers,
    package: offers ? { url: publicUrl(a.package_key), size: a.package_size || 0, sha256: a.package_sha256 || '', version: a.package_version || 1 } : null,
    version: a.version || 0, created: a.created_at || 0, updated: a.updated_at || 0,
    ...(mine ? { mine: true, key: a.seller_key || '', hidden: a.hidden === true } : {}),
  };
}

async function handle(base44: any, body: any): Promise<Response> {
  const action = String(body?.action || '');
  const ads = base44.asServiceRole.entities.StoreAd;

  // Sonify: the computer, and its account when it's signed in.
  const token = String(body?.token || '');
  if (!HEX64.test(token)) return fail(400, 'invalid', 'That request was malformed.');

  const computer = (await base44.asServiceRole.entities.StellaAccount.filter({ token_hash: await sha256Hex(token) }))[0];
  const email = String(computer?.linked_email || '');
  const open = ['list', 'get'].includes(action);

  if (!open && !computer)
    return fail(401, 'unknown', 'Fanan\'s server doesn\'t recognise this computer yet. Restart Sonify while online.');

  if (!open && !email)
    return fail(401, 'sign_in', 'Sign in to your Fanan account first.');

  const findAd = async (id: unknown) => {
    const adId = text(id, 80);
    return AD_ID.test(adId) ? (await ads.filter({ ad: adId }))[0] || null : null;
  };

  const isMine = (a: any) => !!email && a.seller_email === email;

  //--------------------------------------------------------------------------
  if (action === 'list') {
    const kind = String(body?.kind || '');
    const section = text(body?.section, 60);
    const query = text(body?.query, 80).toLowerCase();

    let found = (await ads.filter({ hidden: false }, '-updated_at', 1000))
      .filter((a: any) => a.version > 0 && a.hidden !== true)
      .filter((a: any) => (!kind || a.kind === kind) && (!section || a.section === section))
      .filter((a: any) => !query || [a.title, a.tagline, a.maker, a.story, a.section].some((t) => String(t || '').toLowerCase().includes(query)));

    found.sort((x: any, y: any) => (y.updated_at || 0) - (x.updated_at || 0));

    const total = found.length;
    const pages = Math.max(1, Math.ceil(total / PAGE_SIZE));
    const page = Math.min(pages, Math.max(1, Math.floor(Number(body?.page || 1))));
    found = found.slice((page - 1) * PAGE_SIZE, page * PAGE_SIZE);

    return answer({ ads: found.map((a: any) => publicAd(a, isMine(a))), page, pages, total });
  }

  if (action === 'get') {
    const a = await findAd(body?.ad);
    if (!a || !a.version || (a.hidden === true && !isMine(a))) return fail(404, 'not_found', 'That ad isn\'t on the board.');
    return answer({ ad: publicAd(a, isMine(a)) });
  }

  if (action === 'mine') {
    const own = (await ads.filter({ seller_email: email })).filter((a: any) => a.version > 0);
    own.sort((x: any, y: any) => (y.updated_at || 0) - (x.updated_at || 0));
    return answer({ ads: own.map((a: any) => publicAd(a, true)) });
  }

  //--------------------------------------------------------------------------
  if (action === 'begin') {
    const key = text(body?.key, 80);
    const kind = String(body?.kind || '');
    const title = text(body?.title, MAX_TITLE);

    if (!key || !KINDS.includes(kind) || !title)
      return fail(400, 'invalid', 'The Store needs the ad\'s key, kind and title.');

    // The ad named (unless it was deleted meanwhile), else the one posted before from this project.
    let a = body?.ad ? await findAd(body.ad) : null;

    if (a && a.seller_email !== email)
      return fail(403, 'not_yours', 'That ad is another poster\'s.');

    if (a && a.kind !== kind)
      return fail(400, 'invalid', 'An ad keeps its kind.');

    if (!a)
      a = (await ads.filter({ seller_email: email, seller_key: key }))[0] || null;

    if (!a) {
      if ((await ads.filter({ seller_email: email })).length >= MAX_ADS_PER_SELLER)
        return fail(409, 'too_many', `A poster can have up to ${MAX_ADS_PER_SELLER} ads on the board.`);

      let id = newAdId();
      while ((await ads.filter({ ad: id })).length > 0) id = newAdId();

      const now = Date.now();
      a = await ads.create({
        ad: id, seller_key: key, seller_email: email, kind, title, tagline: '', story: '', features: '[]', section: '', maker: '',
        video: '', website: '', formats: '[]', price_text: '', offers_download: false, picture_key: '', logo_key: '',
        package_key: '', package_size: 0, package_sha256: '', package_version: 0, version: 0, pending_version: 0, pending_at: 0,
        hidden: false, created_at: now, updated_at: now,
      });
    }

    const version = (a.version || 0) + 1;
    await ads.update(a.id, { pending_version: version, pending_at: Date.now() });

    const uploads: Record<string, string> = {
      picture: await uploadUrl(fileKey(a.ad, version, 'picture.png'), 'image/png', MAX_PICTURE),
      logo: await uploadUrl(fileKey(a.ad, version, 'logo.png'), 'image/png', MAX_LOGO),
    };

    if (body?.package === true)
      uploads.package = await uploadUrl(fileKey(a.ad, version, 'package.zip'), 'application/zip', MAX_PACKAGE);

    return answer({ ad: a.ad, version, uploads });
  }

  //--------------------------------------------------------------------------
  if (action === 'publish') {
    const a = await findAd(body?.ad);
    const version = Number(body?.version || 0);

    if (!a) return fail(404, 'not_found', 'The Store doesn\'t know that ad.');
    if (a.seller_email !== email) return fail(403, 'not_yours', 'That ad is another poster\'s.');
    if (!version || version !== a.pending_version) return fail(409, 'stale', 'That post is out of date. Post again.');

    const title = text(body?.title, MAX_TITLE);
    const story = text(body?.story, MAX_STORY);
    const section = text(body?.section, 60);
    const site = website(body?.website);
    const video = youtube(body?.video);
    const offers = body?.offersDownload === true;
    const features = (Array.isArray(body?.features) ? body.features : []).map((f: unknown) => text(f, MAX_FEATURE)).filter(Boolean).slice(0, MAX_FEATURES);
    const formats = FORMATS.filter((f) => (Array.isArray(body?.formats) && body.formats.includes(f)) || (f === 'Sonify' && offers));

    if (!title || !story) return fail(400, 'invalid', 'The ad needs its title and its story.');
    if (!(SECTIONS[a.kind] || []).includes(section)) return fail(400, 'invalid_section', 'Choose the ad\'s section.');
    if (!site) return fail(400, 'invalid_website', 'The ad needs your website\'s address (http or https).');
    if (text(body?.video, MAX_VIDEO) && !video) return fail(400, 'invalid_video', 'The video link isn\'t a YouTube video\'s link.');
    if (formats.length === 0) return fail(400, 'invalid_formats', 'Say which formats it comes in.');

    // The files: the picture always; the logo when it was sent; the package when it was sent
    // (else the one the ad had, if it offers the download).
    const pictureKey = fileKey(a.ad, version, 'picture.png');
    const logoKey = fileKey(a.ad, version, 'logo.png');
    const packageKey = fileKey(a.ad, version, 'package.zip');
    const sentPackage = body?.package && typeof body.package === 'object';

    const [pictureFile, logoFile, packageFile] = await Promise.all([
      fileInfo(pictureKey),
      body?.logo === true ? fileInfo(logoKey) : Promise.resolve(null),
      sentPackage ? fileInfo(packageKey) : Promise.resolve(null),
    ]);

    if (!pictureFile || (body?.logo === true && !logoFile) || (sentPackage && !packageFile))
      return fail(409, 'missing_files', 'The files didn\'t all arrive. Post again.');

    const update: Record<string, unknown> = {
      title, tagline: text(body?.tagline, MAX_TAGLINE), story, section, features: JSON.stringify(features), maker: text(body?.maker, MAX_MAKER),
      video, website: site, formats: JSON.stringify(formats), price_text: text(body?.priceText, MAX_PRICE_TEXT),
      offers_download: offers, picture_key: pictureKey, logo_key: logoFile ? logoKey : '', version,
      pending_version: 0, pending_at: 0, updated_at: Date.now(),
    };

    if (sentPackage) {
      if (packageFile!.size !== Number(body.package.size) || (body.package.sha256 && packageFile!.sha256 !== String(body.package.sha256).toLowerCase()))
        return fail(409, 'damaged', 'The Sonify version arrived damaged. Post again.');

      Object.assign(update, { package_key: packageKey, package_size: packageFile!.size, package_sha256: packageFile!.sha256, package_version: version });
    } else if (offers && !a.package_key) {
      return fail(409, 'no_package', 'The ad offers the Sonify version, but none was sent. Post it from the open project.');
    }

    await ads.update(a.id, update);
    return answer({ ad: a.ad, version });
  }

  //--------------------------------------------------------------------------
  if (action === 'hide' || action === 'unhide' || action === 'delete') {
    const a = await findAd(body?.ad);
    if (!a) return fail(404, 'not_found', 'The Store doesn\'t know that ad.');
    if (a.seller_email !== email) return fail(403, 'not_yours', 'That ad is another poster\'s.');

    if (action === 'delete') {
      await ads.delete(a.id);   // its files stay in R2 (a download already made keeps working)
      return answer({ ad: a.ad, deleted: true });
    }

    await ads.update(a.id, { hidden: action === 'hide' });
    return answer({ ad: a.ad, hidden: action === 'hide' });
  }

  return fail(400, 'invalid', 'That request was malformed.');
}

Deno.serve(async (req) => {
  try {
    const base44 = createClientFromRequest(req);
    const body = await req.json().catch(() => ({}));
    return await handle(base44, body);
  } catch (error) {
    console.error('store failed:', error);
    return fail(500, 'failed', 'The Store couldn\'t do that right now. Please try again.');
  }
});
```

**4. Nothing else changes:** `stellaAccount` (it may keep the `device_hash` field it has; nothing reads it now), `stellaChat`, `sonifyChat`, the voice and remote functions, the other entities and their fields, the wallets, the ledger, the free credits, the packs, the credit purchases with PayPal and every reply format stay exactly as they are. Sonify's Store tab also sends `stellaChat` requests with `mode: 'chat'` and `purpose: 'store_story'` (Fanan AI writes an ad's story from the plugin's facts and a picture of it): they're charged like any chat step, and need no change. Just make sure an image block in `messages` reaches the AI service as it does now.

**5. Test when you're done**, on the published version, with the test computer as before: the token of 64 letters `a` (its `token_hash` is `ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb`), a `StellaAccount` with that hash, linked to `fanan-test@example.com`; and a second one: the token of 64 letters `c` (its `token_hash` is `52b6419d27bd7f547cee3b92f8c17a908b8a49601ecbec161e5030de1dfe9e0a`), linked to `fanan-test2@example.com`.
1. `begin` `{ key: 'test-key', kind: 'effect', title: 'Test Tone', package: true }`. Expect `ok`, an `ad` like `ad-…`, `version` 1 and three upload addresses (`picture`, `logo`, `package`).
2. Send the files, from inside a function: a small PNG to `uploads.picture` and to `uploads.logo` with `content-type: image/png`, and 100 bytes (anything) to `uploads.package` with `content-type: application/zip`. Expect `{ ok: true, size, sha256 }` from each.
3. `publish` `{ ad, version: 1, title: 'Test Tone', tagline: 'A test.', story: 'A test.', section: 'Creative FX', features: ['One'], video: 'https://youtu.be/dQw4w9WgXcQ', website: 'https://example.com/test-tone', formats: ['VST3', 'CLAP'], priceText: '$9, free demo', maker: 'Test', offersDownload: true, picture: true, logo: true, package: { size: 100, sha256: <step 2's package sha256> } }`. Expect `ok`. Then `list` with `{ section: 'Creative FX' }`: expect the ad, with `section` `Creative FX`, `formats` `['VST3', 'CLAP', 'Sonify']`, `offersDownload` true, a `logo` address and a `package` (`version` 1). `list` with `{ section: 'Synths' }`: expect it not there. `get` `{ ad }`: the same; fetch its `package.url` and expect the same 100 bytes.
4. `begin` again `{ key: 'test-key', kind: 'effect', title: 'Test Tone', ad, package: false }`: expect `version` 2 and no `package` upload address. Send only the picture. `publish` version 2 like step 3 but with `website: 'ftp://example.com'`, `logo: false` and no `package`: expect `error` `invalid_website`. Again with `website: 'https://example.com/v2'`: expect `ok`; `get` shows the new website, no logo, and the `package` still at `version` 1.
5. `hide` `{ ad }`: `list` doesn't show it; `get` from the second computer: expect `error` `not_found`; `mine` (first computer): the ad with `hidden` true. `unhide`: `list` shows it again.
6. From the second computer: `hide`, `delete`, and `begin` with the first ad's id (`kind: 'effect'`): expect `error` `not_yours` each time.
7. `delete` `{ ad }` from the first computer: expect `ok`; then `get`: expect `error` `not_found`.
8. Delete the test records (both computers' `StellaAccount`, and any `StoreAd` left). The test files stay in R2 (a few bytes); that's fine.

Tell me: whether the old entities held real records (step 1), anything you had to adapt, and each test's result.
