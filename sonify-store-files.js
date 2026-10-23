// D:\workspace\Fanan_web_hub\relay\sonify-store-files.js
//
// The files of Sonify's store: a Cloudflare Worker in front of an R2 bucket. Plugins and MIDI FX
// posted from Sonify's Protection tab, and their pictures, are kept here; Base44 (the
// sonifyStore function) keeps the catalog and decides who may send what.
//
//   PUT  /upload/<key>?exp=&max=&type=&sig=   a file from Sonify, at an address Base44 signed:
//                                             HMAC-SHA256 with STORE_FILES_SECRET over
//                                             "PUT\n<key>\n<exp>\n<max>\n<type>". Kept with its
//                                             SHA-256, which every answer for it carries.
//   GET  /files/<key>                         a file, for anyone (the store is open to all; a
//   HEAD /files/<key>                         paid plugin plays its demo until it's licensed)
//
// Keys: products/<product>/v<version>/package.zip and .../picture.png. A version's files never
// change once it's listed, so they're cached for good.
//
// Set up in the Cloudflare dashboard:
//   1. R2 > Create bucket: "sonify-store" (private: nothing is public on its own).
//   2. Workers & Pages > Create > Worker: "sonify-store-files". Paste this file and deploy.
//   3. The worker > Settings > Bindings > Add > R2 bucket: variable name STORE, bucket sonify-store.
//   4. The worker > Settings > Variables and Secrets > Add, type Secret: STORE_FILES_SECRET, the
//      same long random text as Base44's STORE_FILES_SECRET secret.
//   5. Base44's STORE_FILES_URL secret: this worker's address (https://sonify-store-files.<your
//      account>.workers.dev), with no slash at the end.

const MAX_PACKAGE = 64 * 1024 * 1024;   // bytes
const MAX_PICTURE = 8 * 1024 * 1024;
const MAX_LIFE_MS = 2 * 60 * 60 * 1000; // a signed address lives at most this long

const KEY = /^products\/[A-Za-z0-9._-]{1,80}\/v[0-9]{1,6}\/(package\.zip|picture\.png)$/;

const CORS = {
  'access-control-allow-origin': '*',
  'access-control-allow-methods': 'GET, HEAD, OPTIONS',
  'access-control-expose-headers': 'content-length, etag, x-content-sha256',
};

function json(body, status = 200) {
  return new Response(JSON.stringify(body), { status, headers: { 'content-type': 'application/json', ...CORS } });
}

function fail(status, error, message) {
  return json({ ok: false, error, message }, status);
}

function hex(buffer) {
  return [...new Uint8Array(buffer)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

async function hmacHex(secret, text) {
  const key = await crypto.subtle.importKey('raw', new TextEncoder().encode(secret), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  return hex(await crypto.subtle.sign('HMAC', key, new TextEncoder().encode(text)));
}

/** The same length or not, in a time that doesn't depend on where they differ. */
function sameText(a, b) {
  if (a.length !== b.length) return false;
  let difference = 0;
  for (let i = 0; i < a.length; ++i) difference |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return difference === 0;
}

function keyOf(url, prefix) {
  try {
    return decodeURIComponent(url.pathname.slice(prefix.length));
  } catch {
    return '';
  }
}

//==============================================================================
async function upload(request, env, url) {
  const key = keyOf(url, '/upload/');
  const exp = Number(url.searchParams.get('exp'));
  const max = Number(url.searchParams.get('max'));
  const type = url.searchParams.get('type') || '';
  const sig = (url.searchParams.get('sig') || '').toLowerCase();
  const isPackage = key.endsWith('/package.zip');
  const now = Date.now();

  if (!KEY.test(key) || !Number.isFinite(exp) || !Number.isFinite(max) || max <= 0 || !/^[0-9a-f]{64}$/.test(sig))
    return fail(400, 'invalid', 'That address is malformed.');

  if (type !== (isPackage ? 'application/zip' : 'image/png') || max > (isPackage ? MAX_PACKAGE : MAX_PICTURE))
    return fail(400, 'invalid', 'That address is malformed.');

  if (!env.STORE_FILES_SECRET || !env.STORE)
    return fail(500, 'not_set_up', 'The store\'s files aren\'t set up yet.');

  if (!sameText(sig, await hmacHex(env.STORE_FILES_SECRET, `PUT\n${key}\n${exp}\n${max}\n${type}`)))
    return fail(403, 'forbidden', 'That address isn\'t the store\'s.');

  if (exp < now || exp > now + MAX_LIFE_MS)
    return fail(403, 'expired', 'That address has expired. Post again.');

  const declared = Number(request.headers.get('content-length'));

  if (Number.isFinite(declared) && declared > max)
    return fail(413, 'too_big', 'The file is too big for the store.');

  const data = await request.arrayBuffer();

  if (data.byteLength === 0 || data.byteLength > max)
    return fail(data.byteLength === 0 ? 400 : 413, data.byteLength === 0 ? 'empty' : 'too_big',
                data.byteLength === 0 ? 'The file was empty.' : 'The file is too big for the store.');

  const sha256 = hex(await crypto.subtle.digest('SHA-256', data));

  await env.STORE.put(key, data, {
    httpMetadata: { contentType: type, cacheControl: 'public, max-age=31536000, immutable' },
    customMetadata: { sha256 },
  });

  return json({ ok: true, size: data.byteLength, sha256 });
}

async function serve(request, env, url) {
  const key = keyOf(url, '/files/');

  if (!KEY.test(key))
    return fail(404, 'not_found', 'There\'s no such file.');

  const head = request.method === 'HEAD';
  const object = head ? await env.STORE.head(key) : await env.STORE.get(key);

  if (!object)
    return fail(404, 'not_found', 'There\'s no such file.');

  const headers = new Headers(CORS);
  object.writeHttpMetadata(headers);
  headers.set('etag', object.httpEtag);
  headers.set('content-length', String(object.size));
  headers.set('x-content-sha256', (object.customMetadata && object.customMetadata.sha256) || '');

  if (key.endsWith('/package.zip'))
    headers.set('content-disposition', 'attachment; filename="package.zip"');

  return new Response(head ? null : object.body, { status: 200, headers });
}

//==============================================================================
export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    try {
      if (request.method === 'OPTIONS')
        return new Response(null, { status: 204, headers: CORS });

      if (url.pathname.startsWith('/upload/') && request.method === 'PUT')
        return await upload(request, env, url);

      if (url.pathname.startsWith('/files/') && (request.method === 'GET' || request.method === 'HEAD'))
        return await serve(request, env, url);

      if (url.pathname === '/' && request.method === 'GET')
        return new Response('Sonify store files', { status: 200, headers: { 'content-type': 'text/plain' } });

      return fail(404, 'not_found', 'There\'s nothing here.');
    } catch (error) {
      console.log('store files failed:', String((error && (error.stack || error.message)) || error));
      return fail(500, 'failed', 'The store\'s files couldn\'t do that right now. Please try again.');
    }
  },
};
