-- D:\workspace\Fanan_web_hub\src\cloud\tomahawk_library\schema.sql
--
-- The Tomahawk library Worker's daily counts (worker.js): one row per day and per computer,
-- address or everyone together. Paste into the D1 database's Console once and run it.

CREATE TABLE IF NOT EXISTS usage (
  day      TEXT    NOT NULL,             -- YYYY-MM-DD, UTC
  kind     TEXT    NOT NULL,             -- computer, address or all
  id       TEXT    NOT NULL,             -- the computer's id, the address (IPv6 by /64), or "all"
  bytes    INTEGER NOT NULL DEFAULT 0,   -- downloaded that day
  requests INTEGER NOT NULL DEFAULT 0,   -- file requests that day
  PRIMARY KEY (day, kind, id)
);
