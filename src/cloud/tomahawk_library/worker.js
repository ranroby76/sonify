// D:\workspace\Fanan_web_hub\src\cloud\tomahawk_library\worker.js
//
// The Tomahawk library's downloads: a Cloudflare Worker in front of the private R2 bucket
// (tomahawk-sampler). Sonify asks it for the library's list and for the files; the bucket
// itself is never public.
//
//   GET /index            the library's files: { version, files: [{ path, size, md5 }] },
//                          read from the bucket (one listing) and kept for an hour
//   GET /file/<path>       one file (a Range for the rest of an interrupted download), counted
//                          against the computer's and the address's daily allowance
//   HEAD /file/<path>      its size and checksum, counted as nothing
//
// Who asks: Sonify sends X-Sonify-Id, the SHA-256 of its computer key (never the key itself).
// Without one, the address alone counts.
//
// Daily allowances, from midnight UTC, each a setting of the Worker (Settings > Variables):
//   PER_COMPUTER_GB   per computer        (default 42: three whole libraries, and a little over)
//   PER_ADDRESS_GB    per internet address (default 84: six whole libraries, two computers' worth)
//   PER_DAY_REQUESTS  file requests per computer or address (default 5000)
//   ALL_DAY_GB        everyone together; 0 is no limit (default 0)
// Over one, the answer is 429 with { error: "limit", retryAfter } until midnight UTC.
//
// Bindings (Settings > Bindings): LIBRARY (the R2 bucket), USAGE (the D1 database made
// with schema.sql). Library version (the bucket's top folder): VERSION (default "v1").

const GB = 1024 * 1024 * 1024;
const INDEX_SECONDS = 3600;

export default {
  async fetch(request, env, ctx) {
    try {
      const url = new URL(request.url);

      if (request.method !== "GET" && request.method !== "HEAD")
        return json({ error: "method" }, 405, { Allow: "GET, HEAD" });

      if (url.pathname === "/index")
        return await libraryIndex(request, env, ctx);

      if (url.pathname.startsWith("/file/"))
        return await libraryFile(request, env, ctx, url);

      return json({ error: "not_found" }, 404);
    } catch (error) {
      console.error(error && error.stack ? error.stack : String(error));
      return json({ error: "failed" }, 500);
    }
  }
};

// ------------------------------------------------------------------ settings
function settings(env) {
  const number = (value, fallback) => {
    const n = Number(value);
    return Number.isFinite(n) && n >= 0 ? n : fallback;
  };

  return {
    version: String(env.VERSION || "v1").replace(/[^A-Za-z0-9_-]/g, "") || "v1",
    perComputer: number(env.PER_COMPUTER_GB, 42) * GB,
    perAddress: number(env.PER_ADDRESS_GB, 84) * GB,
    perDayRequests: number(env.PER_DAY_REQUESTS, 5000),
    allDay: number(env.ALL_DAY_GB, 0) * GB
  };
}

// ------------------------------------------------------------------ the list
async function libraryIndex(request, env, ctx) {
  const { version } = settings(env);
  const cache = caches.default;
  const key = new Request(`https://tomahawk-library.cache/index/${version}`);

  const kept = await cache.match(key);
  if (kept) return kept;

  const files = [];
  let cursor;

  do {
    const page = await env.LIBRARY.list({ prefix: `${version}/`, cursor, limit: 1000 });

    for (const object of page.objects) {
      const path = object.key.slice(version.length + 1);
      if (!path || path.endsWith("/")) continue;
      files.push({ path, size: object.size, md5: md5Of(object) });
    }

    cursor = page.truncated ? page.cursor : undefined;
  } while (cursor);

  files.sort((a, b) => (a.path < b.path ? -1 : a.path > b.path ? 1 : 0));

  const response = json({ version, files }, 200, { "Cache-Control": `public, max-age=${INDEX_SECONDS}` });
  ctx.waitUntil(cache.put(key, response.clone()));
  return response;
}

/** The file's MD5 (hex): R2 keeps it for files uploaded in one piece, as rclone does under 200 MB. */
function md5Of(object) {
  if (object.checksums && object.checksums.md5) {
    const bytes = new Uint8Array(object.checksums.md5);
    return [...bytes].map((b) => b.toString(16).padStart(2, "0")).join("");
  }

  const etag = String(object.etag || "").replace(/"/g, "");
  return /^[0-9a-f]{32}$/i.test(etag) ? etag.toLowerCase() : "";
}

// ------------------------------------------------------------------ one file
async function libraryFile(request, env, ctx, url) {
  const config = settings(env);
  let path;

  try {
    path = decodeURIComponent(url.pathname.slice("/file/".length));
  } catch {
    return json({ error: "bad_path" }, 400);
  }

  // Only the library's own files: no way up and out of its folder.
  if (!path || path.length > 512 || path.includes("..") || path.startsWith("/") || path.includes("\\"))
    return json({ error: "bad_path" }, 400);

  const key = `${config.version}/${path}`;
  const head = await env.LIBRARY.head(key);

  if (!head)
    return json({ error: "not_found" }, 404);

  const md5 = md5Of(head);
  const common = {
    "Accept-Ranges": "bytes",
    "ETag": `"${head.etag}"`,
    "Content-Type": "application/octet-stream",
    "Content-Disposition": `attachment; filename="${asciiName(path)}"`,
    "X-Content-MD5": md5,
    "Cache-Control": "private, no-store"
  };

  if (request.method === "HEAD")
    return new Response(null, { status: 200, headers: { ...common, "Content-Length": String(head.size) } });

  // The part asked for: the whole file, or the rest of it (a download carrying on).
  const range = parseRange(request.headers.get("Range"), head.size);

  if (range === "bad")
    return new Response(null, { status: 416, headers: { ...common, "Content-Range": `bytes */${head.size}` } });

  const length = range ? range.end - range.start + 1 : head.size;

  // Counted before it's sent; over an allowance, nothing is read from the bucket.
  const verdict = await countDownload(env, request, config, length);

  if (verdict)
    return verdict;

  const object = await env.LIBRARY.get(key, range ? { range: { offset: range.start, length } } : undefined);

  if (!object)
    return json({ error: "not_found" }, 404);

  maybeTidy(env, ctx);

  if (range)
    return new Response(object.body, {
      status: 206,
      headers: { ...common, "Content-Length": String(length), "Content-Range": `bytes ${range.start}-${range.end}/${head.size}` }
    });

  return new Response(object.body, { status: 200, headers: { ...common, "Content-Length": String(head.size) } });
}

/** "bytes=START-" or "bytes=START-END": { start, end } (inclusive); null for none; "bad" when it can't be served. */
function parseRange(header, size) {
  if (!header) return null;
  const match = /^bytes=(\d*)-(\d*)$/.exec(header.trim());
  if (!match || (match[1] === "" && match[2] === "")) return "bad";

  let start, end;

  if (match[1] === "") {                  // the last N bytes
    const suffix = Number(match[2]);
    if (suffix <= 0) return "bad";
    start = Math.max(0, size - suffix);
    end = size - 1;
  } else {
    start = Number(match[1]);
    end = match[2] === "" ? size - 1 : Math.min(Number(match[2]), size - 1);
  }

  if (!Number.isSafeInteger(start) || !Number.isSafeInteger(end) || start > end || start >= size) return "bad";
  return start === 0 && end === size - 1 ? null : { start, end };
}

function asciiName(path) {
  return path.split("/").pop().replace(/[^A-Za-z0-9._()-]/g, "_");
}

// ------------------------------------------------------------------ the daily allowances
const today = () => new Date().toISOString().slice(0, 10);

function secondsToMidnight() {
  const now = new Date();
  const midnight = Date.UTC(now.getUTCFullYear(), now.getUTCMonth(), now.getUTCDate() + 1);
  return Math.max(1, Math.ceil((midnight - now.getTime()) / 1000));
}

/** The address as it counts: an IPv6 address by its /64 (one home's whole block). */
function addressOf(request) {
  const ip = request.headers.get("CF-Connecting-IP") || "unknown";
  if (!ip.includes(":")) return ip;
  const groups = ip.split("::")[0].split(":");
  return groups.slice(0, 4).join(":") + "::/64";
}

function computerOf(request) {
  const id = String(request.headers.get("X-Sonify-Id") || "").trim().toLowerCase();
  return /^[0-9a-f]{64}$/.test(id) ? id : "";
}

/**
 * Adds the download to today's counts (the computer's, the address's, everyone's), each only
 * if it stays within its allowance. Nothing over: null. One over: what's counted is taken back,
 * and the answer is 429.
 */
async function countDownload(env, request, config, bytes) {
  const day = today();
  const counted = [];
  const who = [];

  const computer = computerOf(request);
  if (computer) who.push(["computer", computer, config.perComputer]);
  who.push(["address", addressOf(request), config.perAddress]);

  for (const [kind, id, limit] of who) {
    if (!(await add(env, day, kind, id, bytes, limit, config.perDayRequests))) {
      await takeBack(env, day, counted, bytes);
      return over(kind);
    }
    counted.push([kind, id]);
  }

  if (config.allDay > 0) {
    if (!(await add(env, day, "all", "all", bytes, config.allDay, 0))) {
      await takeBack(env, day, counted, bytes);
      return over("all");
    }
  }

  return null;
}

/** One row's count up by bytes (and one request), in a single statement so it holds when many arrive at once. */
async function add(env, day, kind, id, bytes, limit, maxRequests) {
  if (bytes > limit) return false;

  const row = await env.USAGE.prepare(
    `INSERT INTO usage (day, kind, id, bytes, requests) VALUES (?1, ?2, ?3, ?4, 1)
     ON CONFLICT (day, kind, id) DO UPDATE SET bytes = bytes + ?4, requests = requests + 1
     WHERE usage.bytes + ?4 <= ?5 AND (?6 = 0 OR usage.requests + 1 <= ?6)
     RETURNING bytes`
  ).bind(day, kind, id, bytes, limit, maxRequests).first();

  return row !== null;
}

async function takeBack(env, day, counted, bytes) {
  for (const [kind, id] of counted)
    await env.USAGE.prepare(
      `UPDATE usage SET bytes = MAX(0, bytes - ?4), requests = MAX(0, requests - 1) WHERE day = ?1 AND kind = ?2 AND id = ?3`
    ).bind(day, kind, id, bytes).run();
}

function over(kind) {
  const retryAfter = secondsToMidnight();
  const message = kind === "all"
    ? "The library's downloads are paused for today. Please try again tomorrow."
    : "Today's download allowance is used up. Please try again tomorrow.";
  return json({ error: "limit", who: kind, retryAfter, message }, 429, { "Retry-After": String(retryAfter) });
}

/** Now and then, the counts from before yesterday go. */
function maybeTidy(env, ctx) {
  if (Math.random() > 0.005) return;
  const yesterday = new Date(Date.now() - 86400000).toISOString().slice(0, 10);
  ctx.waitUntil(env.USAGE.prepare(`DELETE FROM usage WHERE day < ?1`).bind(yesterday).run());
}

// ------------------------------------------------------------------ answers
function json(body, status = 200, headers = {}) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "Content-Type": "application/json; charset=utf-8", ...headers }
  });
}
