<!-- D:\workspace\Fanan_web_hub\src\base44_hub_content_prompt.md -->
# Base44 prompt: Fanan content for the Hub

Paste everything below the line into Base44's AI chat, in the **same Fanan Hub Cloud app** as the accounts prompt.

---

Add published content to this app: loops, samples, songs, plugins and MIDI tools that Fanan makes available to everyone using the Fanan Web Hub desktop app. The desktop app reads it through the Base44 API as an external app.

**Entity: FananContent** (one record per published item)
- `title`: text, required
- `kind`: one of `loop`, `sample`, `song`, `plugin`, `miditool`, required
- `description`: text
- `tags`: array of text
- `bpm`: number (loops)
- `key`: text (loops and samples, e.g. "E minor")
- `version`: text, required (e.g. "1.0.2"; raise it when a file is replaced, so users see Update)
- `file_url`: text, required (the public link of the uploaded file)
- `preview_url`: text (optional short audio preview, public link)
- `size_bytes`: number (the file's size)
- `tool_id`: text (MIDI tools only: the tool's id, lowercase letters, digits, dots and dashes, e.g. `magenta.melody-rnn`)
- `published`: true/false, default false

**Security for FananContent**
- Read: everyone, including anonymous visitors (no sign-in needed).
- Create, update, delete: admins only.

**Admin page: "Publish content"** (admins only)
- A list of all FananContent records, newest first, with their published state.
- A form to add or edit an item: upload the file (public upload, `UploadFile`), which fills `file_url` and `size_bytes` automatically; optionally upload a preview; fill title, kind, description, tags, bpm, key, version and (for MIDI tools) tool_id; a Published switch.
- Replacing an item's file must also raise its version.

---

**Packaging a song:** zip the song's folder, the `.fsong` file together with its `Media` folder, and upload the zip as the item's file with kind `song`.

**Plugins:** only WCLAP files (`.wclap.tar.gz` or `.wasm`). The Hub installs nothing else.

**MIDI tools:** a Magenta model for Sonify's MIDI tab, kind `miditool`, its `tool_id` set. The file is either:
- a definition, `tool.json`: `{"id": "magenta.melody-rnn", "title": "Continue Melody", "description": "...", "model": "MusicRNN" | "MusicVAE" | "Coconet", "checkpoint": "https://.../basic_rnn", "actions": ["continue"], "live": "continue", "drums": false, "generator": false, "version": "2"}`. Sonify downloads the model from `checkpoint` (a folder holding `config.json`, `weights_manifest.json` and the weight files the manifest lists; Coconet has no `config.json`). Actions are `continue`, `generate`, `vary`, `morph`, `groove`, `drumify`, `improvise` and `harmonize`; `live` is what the tool does on a track's MIDI FX.
- or a package, a `.zip` holding that `tool.json` and a `model` folder with the model's files (no download from elsewhere).

A `tool_id` that is one of Sonify's built-in Magenta tools (`magenta.melody-rnn`, `magenta.drums-rnn`, `magenta.improvise`, `magenta.melody-vae`, `magenta.drums-vae`, `magenta.groove`, `magenta.drumify`, `magenta.harmonize`) with a higher `version` than `1` shows as an Update for that tool; any other id adds a new tool.
