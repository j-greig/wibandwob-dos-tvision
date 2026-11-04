FastAPI MVP for Programmatic Control API (v1)

Overview
- Local-only FastAPI server that implements the v1 API described in `prds/programmatic-control-api-prd.md`.
- In-memory controller models windows, layout, menu commands, properties, workspace, screenshots, and pattern mode.
- WebSocket event stream broadcasts state changes for reactive clients (humans or LLM tools).
- Designed for a clean swap to an IPC or in-process adapter that talks to the Turbo Vision app.

Structure
- `tools/api_server/main.py`: FastAPI app, routes, and WebSocket endpoint.
- `tools/api_server/controller.py`: In-memory controller; replace with C++ IPC/in-process bridge later.
- `tools/api_server/models.py`: Domain entities (Window, Rect, AppState, WindowType).
- `tools/api_server/schemas.py`: Pydantic request/response models (create, move, props, state, etc.).
- `tools/api_server/events.py`: Broadcast hub for `/ws` clients; in-process listeners (the show bus).
- `tools/api_server/show.py`: show endpoints, `/events` SSE, capture timeline (see Show endpoints).
- `tools/api_server/requirements.txt`: Python dependencies.
- `tools/api_server/README.md`: This guide.

Install and Run
- Python: 3.9+ (3.11 recommended). The code is 3.9-compatible.
- Create venv in API server dir: `cd tools/api_server && python3 -m venv venv`
- Activate venv: `source venv/bin/activate`
- Install deps: `pip install -r requirements.txt`
- **IMPORTANT**: Run from project root: `cd /path/to/tvision`
- Start server: `python -m tools.api_server.main --port=8089` (binds `127.0.0.1:8089`)
  - Or use venv directly: `./tools/api_server/venv/bin/python -m tools.api_server.main --port=8089`
- OpenAPI docs: `http://127.0.0.1:8089/docs`
- WebSocket: `ws://127.0.0.1:8089/ws`
- MCP endpoint: `http://127.0.0.1:8089/mcp` (requires `fastapi-mcp` installed)
- Deactivate venv when finished: `deactivate`

Alternative using uv (faster):
- `cd tools/api_server && uv venv && uv pip install -r requirements.txt`
- `cd ../.. && uv run python -m tools.api_server.main --port=8089`

Run the C++ App (for live control)
- Build once (from repo root): `cmake -S . -B build && cmake --build build -j`
- Build test app (from `test-tui`): `mkdir -p build && cd build && cmake .. && cmake --build . -j`
- Run the app: `./wwdos`
- Verify socket exists (new terminal): `ls -l /tmp/wwdos.sock`

Window Types
- **test_pattern** — Test pattern generator with gradients and wallpaper (protocol slug)
- **gradient** — Gradient displays: `props: {"gradient": "horizontal|vertical|radial|diagonal"}`
- **frame_player** — Animation player: `props: {"path": "file.txt", "fps": 10}`
- **text_view** — Text/primer file viewer: `props: {"path": "file.txt"}` (readonly)
- **text_editor** — Interactive text editor (supports send_text/send_figlet APIs)
- **wallpaper** — Static wallpaper patterns

IPC Bridge to the C++ App
- The C++ `wwdos` app creates a Unix socket at `/tmp/wwdos.sock`.
- This FastAPI server connects to that socket and forwards commands.
- Multi-instance: set `WIBWOB_INSTANCE=N` on both TUI and API → `/tmp/wibwob_N.sock`.

Key Endpoints
- Health: `GET /health` — basic liveness.
- Capabilities: `GET /capabilities` — window types, commands, properties schema.
- State: `GET /state` — `pattern_mode`, `windows`, `canvas`, `last_workspace`, `last_screenshot`, `uptime_sec`.
- Window lifecycle:
  - `POST /windows` — create: `{type, title?, rect?, props?}`
  - `POST /windows/{id}/move` — move/resize: `{x?, y?, w?, h?}`
  - `POST /windows/{id}/focus` — bring to front and focus
  - `POST /windows/{id}/clone` — duplicate window and props
  - `POST /windows/{id}/close` — close a window
  - `POST /windows/cascade` — cascade layout
  - `POST /windows/tile` — tile layout: `{cols?}`
  - `POST /windows/close_all` — close all windows
- Text editor API:
  - `POST /windows/{id}/send_text` — send text/ASCII to text editor: `{content, mode?, position?}`
  - `POST /text_editor/send_text` — auto-spawn text editor if needed: `{content, mode?, position?}`
  - `POST /windows/{id}/send_figlet` — send FIGlet ASCII art: `{text, font?, width?, mode?}`
  - `POST /text_editor/send_figlet` — auto-spawn text editor for FIGlet: `{text, font?, width?, mode?}`
  - `POST /windows/{id}/send_multi_figlet` — multi-font FIGlet segments: `{segments, separator?, mode?}`
- Properties: `POST /props/{id}` — update window props: `{props:{...}}`
- Menu commands: `POST /menu/command` — `{command, args?}` (supports `cascade`, `tile`, `close_all`, `save_workspace`, `open_workspace`, `screenshot`)
- Workspace: `POST /workspace/save|open` — `{path}`
- Browser copy: `POST /browser/{window_id}/copy` — `{format:"plain|markdown|tui", include_image_urls?, image_url_mode?}`
- Screenshot: `POST /screenshot` — `{path?}` returns capture metadata (`path`, `backend`, `bytes`, `file_exists`) from canonical `.txt`/`.ans` capture and fails if output file is missing
- Pattern mode: `POST /pattern_mode` — `{mode:"continuous"|"tiled"}`
- **Batch layout: `POST /windows/batch_layout`** — create/move/close multiple windows in one call with macro support
- **Timeline operations: `GET /timeline/status?group_id=...`, `POST /timeline/cancel`** — for future scheduled operations 
- **Primer discovery: `GET /primers/list`** — list all available primer files with metadata (128 primers available)
- **Batch primers: `POST /primers/batch`** — spawn up to 20 primer windows at once with auto-sizing
- WebSocket events: `GET /ws`

WebSocket Events
- `window.created` — `{id, type, title, rect, z, focused, props}`
- `window.updated` — same payload as created
- `window.closed` — `{id}`
- `layout.cascade` — `{}`
- `layout.tile` — `{cols}`
- `command.executed` — `{command, ok, ...}`
- `workspace.saved` — `{path}`
- `workspace.opened` — `{path}`
- `screenshot.saved` — `{path}`
- `pattern.mode` — `{mode}`

Examples (curl)
- Create a test pattern window:
  - `curl -X POST localhost:8089/windows -H 'Content-Type: application/json' -d '{"type":"test_pattern","title":"TP #1","rect":{"x":2,"y":1,"w":40,"h":12}}'`
- Create text editor and send ASCII art:
  - `curl -X POST localhost:8089/text_editor/send_figlet -H 'Content-Type: application/json' -d '{"text":"TURBO TEXT","font":"slant","mode":"replace"}'`
- Send text to existing text editor:
  - `curl -X POST localhost:8089/windows/<id>/send_text -H 'Content-Type: application/json' -d '{"content":"Hello world!\n","mode":"append"}'`
- Load primer file as text view:
  - `curl -X POST localhost:8089/windows -H 'Content-Type: application/json' -d '{"type":"text_view","props":{"path":"primers/monster-emoji.txt"}}'`
- Batch spawn primers with positioning:
  - `curl -X POST localhost:8089/primers/batch -H 'Content-Type: application/json' -d '{"primers":[{"primer_path":"primers/monster-angel-of-death.txt","x":10,"y":5},{"primer_path":"primers/iso-disco-cubes.txt","x":50,"y":5}]}'`
- Tile all windows (2 columns):
  - `curl -X POST localhost:8089/windows/tile -H 'Content-Type: application/json' -d '{"cols":2}'`
- Update frame player FPS:
  - `curl -X POST localhost:8089/props/<id> -H 'Content-Type: application/json' -d '{"props":{"fps":24}}'`
- Execute a menu command (save workspace):
  - `curl -X POST localhost:8089/menu/command -H 'Content-Type: application/json' -d '{"command":"save_workspace","args":{"path":"workspace.json"}}'`

### Browser Copy API
Copy browser content to system clipboard without requiring TUI focus:

```bash
curl -X POST "http://127.0.0.1:8089/browser/<window_id>/copy" \
  -H "Content-Type: application/json" \
  -d '{
    "format": "markdown",
    "include_image_urls": true,
    "image_url_mode": "full"
  }'
```

Response example:

```json
{
  "ok": true,
  "window_id": "win_abc123",
  "chars": 1242,
  "copied": true,
  "format": "markdown",
  "included_image_urls": 3
}
```

Format semantics:
- `plain`: TUI content with ANSI escape sequences stripped
- `markdown`: browser markdown payload
- `tui`: rendered TUI payload (may include ANSI)

Image URL behavior:
- `include_image_urls=true` appends an `Image URLs` section built from browser asset metadata.
- `image_url_mode=full` outputs absolute URLs (resolved against current browser URL when needed).

Error cases (HTTP 400):
- Browser window not found / wrong window type
- Empty browser content for requested format
- Clipboard unavailable or write failure
- Invalid `format` or `image_url_mode`

## Text Editor & Primer Files

### Text Editor Features
The text_editor window type supports real-time text injection via API:
- **Live text streaming**: Send content while user interacts with editor
- **ASCII art integration**: Built-in FIGlet font support for headers/banners  
- **Multiple modes**: append (add to end), replace (clear all), insert (at cursor)
- **Auto-spawn**: API endpoints auto-create editor if none exists

### FIGlet ASCII Art Fonts
Popular fonts include: `standard`, `slant`, `big`, `small`, `banner`, `bubble`, `doom`, `starwars`
```bash
# Send ASCII art header
curl -X POST localhost:8089/text_editor/send_figlet \
  -H 'Content-Type: application/json' \
  -d '{"text":"TURBO TEXT","font":"slant","mode":"replace"}'
```

### Primer File Loading
Load pre-made ASCII art/animations from `test-tui/primers/`:
```bash
# Monster emoji art
curl -X POST localhost:8089/windows -H 'Content-Type: application/json' \
  -d '{"type":"text_view","props":{"path":"primers/monster-emoji.txt"}}'

# Wibwob portrait 
curl -X POST localhost:8089/windows -H 'Content-Type: application/json' \
  -d '{"type":"text_view","props":{"path":"primers/wibwob-portrait-2.txt"}}'

# Animated ASCII (frame format)
curl -X POST localhost:8089/windows -H 'Content-Type: application/json' \
  -d '{"type":"frame_player","props":{"path":"primers/ascii-draws-itself.txt","fps":8}}'
```

Available primer files: `perception.txt`, `monster-emoji.txt`, `wibwob-portrait-2.txt`, `ascii-draws-itself.txt`

### Primer Discovery API
Discover all available primer files with metadata:
```bash
# List all primer files
curl -X GET localhost:8089/primers/list

# Response includes name, path, and file size
{
  "primers": [
    {"name": "monster-emoji", "path": "primers/monster-emoji.txt", "size_kb": 3.3},
    {"name": "perception", "path": "primers/perception.txt", "size_kb": 5.4}
  ],
  "count": 128
}
```

### Batch Primer Spawning
Spawn up to 20 primer windows at specified positions:
```bash
# Batch spawn primers with precise positioning
curl -X POST localhost:8089/primers/batch -H 'Content-Type: application/json' \
  -d '{
    "primers": [
      {"primer_path": "primers/monster-emoji.txt", "x": 10, "y": 5},
      {"primer_path": "primers/space-cat.txt", "x": 60, "y": 15},
      {"primer_path": "primers/hypersigil-mesh.txt", "x": 100, "y": 25}
    ]
  }'

# Windows auto-size based on content, only position is required
# Returns list of created windows with their final dimensions
```

### Browser/State Dump Helper
Capture a browser fetch bundle and app state to timestamped files for agent consumption:
```bash
# Basic: key-inline + all-inline bundles, plus /state
tools/api_server/dump_browser_bundle.sh https://symbient.life

# Include direct edge markdown probe (Accept: text/markdown)
tools/api_server/dump_browser_bundle.sh \
  https://blog.cloudflare.com/markdown-for-agents/ \
  --probe-markdown-edge
```

Outputs are written to `logs/browser/dumps/<timestamp>/` and include:
- raw API responses
- normalized bundle JSON (`.bundle // .`)
- extracted `tui_text` and markdown text files
- compact asset summaries
- optional state snapshot and edge markdown probe headers/body

## Batch Layout (LLM-Optimized)

The batch layout endpoint enables AI agents to create complex window arrangements in a single call, following MCP "second wave" principles:

### Create a 3x2 Grid of Test Pattern Windows
```bash
curl -X POST localhost:8089/windows/batch_layout \
  -H 'Content-Type: application/json' \
  -d '{
    "request_id": "grid-demo-001",
    "group_id": "demo-scene", 
    "defaults": {"view_type": "test_pattern"},
    "ops": [{
      "op": "macro.create_grid",
      "title": "Demo Window",
      "grid": {
        "cols": 3, "rows": 2,
        "cell_w": 25, "cell_h": 8,
        "gap_x": 1, "gap_y": 1,
        "origin": {"x": 2, "y": 2, "w": 0, "h": 0},
        "order": "row_major"
      }
    }]
  }'
```

### Mixed Operations (Create + Move + Close)
```bash
curl -X POST localhost:8089/windows/batch_layout \
  -H 'Content-Type: application/json' \
  -d '{
    "request_id": "mixed-ops-001", 
    "ops": [
      {
        "op": "create",
        "view_type": "gradient",
        "title": "Rainbow Gradient",
        "bounds": {"x": 10, "y": 3, "w": 30, "h": 10},
        "options": {"gradient": "diagonal"}
      },
      {
        "op": "move_resize", 
        "window_id": "existing_window_id",
        "bounds": {"x": 45, "y": 3, "w": 30, "h": 10}
      },
      {
        "op": "close",
        "window_id": "another_window_id"
      }
    ]
  }'
```

### Dry Run (Simulate Without Applying)
```bash
curl -X POST localhost:8089/windows/batch_layout \
  -H 'Content-Type: application/json' \
  -d '{
    "request_id": "preview-001",
    "dry_run": true,
    "defaults": {"view_type": "gradient"},
    "ops": [{
      "op": "macro.create_grid",
      "grid": {"cols": 4, "rows": 3, "cell_w": 20, "cell_h": 6}
    }]
  }'
```

### Response Format
All batch operations return:
```json
{
  "dry_run": false,
  "applied": true,
  "group_id": "demo-scene",
  "op_results": [
    {
      "status": "applied",
      "window_id": "win_abc123",
      "final_bounds": {"x": 2, "y": 2, "w": 25, "h": 8}
    }
  ],
  "warnings": [],
  "timeline_summary": {"counts": {"ops": 6}}
}
```

## Show endpoints (live desktop performance)

Code: `tools/api_server/show.py`. Contract tests (no app needed): `tools/api_server/venv/bin/python -m unittest tests/test_show_api.py`.
Proof run: `python3 scripts/glitch-show-demo.py --api http://127.0.0.1:8097 --run /tmp/run`, then
`uv run tools/frames_to_png.py /tmp/run --out <dir> --beats --contact --mp4 show.mp4`.
`actor` is `wib`, `wob` or `scramble` everywhere; it moves that voice's cursor first and tags the events.

| endpoint | body | does |
|---|---|---|
| `POST /windows/{id}/send_text` | `+ cps, actor` | with `cps`: typed reveal, one key at a time (jitter, longer pause after punctuation). Without `cps`: unchanged paste |
| `POST /windows/{id}/type` | `content, cps=14, target=editor\|title\|figlet, mode, jitter, actor` | typed reveal into an editor, a window title, or a figlet window |
| `POST /windows/{id}/move` | `+ ms, ease, actor` | with `ms`: glide there (`ease` linear\|inout\|out\|back\|bounce, ~24 steps/s, w/h tween too). Without `ms`: unchanged jump |
| `POST /windows/{id}/glide` | `x, y, w?, h?, ms=800, ease, fps=24, actor` | the same glide as its own route |
| `POST /cursors/on` | `{on: true}` | draw the hands on the desktop as tiny frameless windows (`wib` つ◕, `wob` ┼○, `scramble` ᐠᐟ); `{on:false}` removes them. Cursor events stream either way |
| `POST /cursors/{actor}` | `x, y, ms=350, ease, glyph?` | move that hand |
| `POST /windows/shape` | `mask, cell="WxH"?, x?, y?, fill=""\|"mask"\|char, primers[], titles[], order=rows\|cols\|random\|spiral, stagger_ms=60, frameless, actor` | one window per non-space, non-`.` mask cell; the frames draw the mask. `mask` is text or a built-in name; `GET /windows/shape/masks` lists them (scramble, tower, twins, heart) |
| `POST /windows/fall` | `ids[]` (empty = all but cursors), `g=70, bounce=0.3, repose=2, jitter=5, fps=24, max_s=8, seed, actor` | gravity: windows drop, bounce, pile and spill (vendored `gridphysics.py`, a window raises the pile by its full height) |
| `POST /windows/droste` | `depth=6, scale=0.55, x?, y?, w?, h?, title, pause_ms=600, last="   z\n/ᐠ -w- ᐟ\\", actor` | the screen captured into a window, that window captured into a smaller one inside it, down to `last` (Scramble asleep) |
| `POST /chrome` | `menu?, status?, kaomoji?, reset, cps, actor` | write the menu bar, status line and menu-bar kaomoji (`cps` types it in). Needs the build with `chrome_set`; older builds answer 501 |
| `POST /paint/bare` | `win_id, on=true` | paint window as canvas only: no menu, tool panel, palette or status (clears the canvas) |
| `POST /capture/start` | `dir?, name, settle_ms=40, every={"move.step":3,"type":3,"fall.step":2,"cursor":2}` | start a capture timeline: every action writes `NNNNN-<type>-<actor>.txt` (screen text) into the run dir, plus `events.jsonl` and `frames.jsonl` (time, beat). Default dir `logs/show/<stamp>-<name>/` |
| `POST /capture/frame` `{label}` · `POST /capture/stop` · `GET /capture/status` | | extra frame, stop (returns run dir and count), status |
| `POST /events/mark` | `label, text?, actor?` | a marker on the stream; frames after it belong to that beat (`frames_to_png.py --beats` takes the last frame of each) |
| `GET /events` | `?types=open,land` | Server-Sent Events, one per action (below). `GET /events/recent?n=&after=` polls the last 2000 |

### Stage endpoints: EGA theme, window colours, film windows, nested desktops

Code: `tools/api_server/show_stage.py` (film, nest), `app/ega_theme.cpp` (palette, window colours),
`tools/term_exec.sh` (the program of an exec terminal). Contract tests (no app needed):
`tools/api_server/venv/bin/python -m unittest tests/test_show_stage.py`. Proof PNGs of a live run:
`uv run tools/ansi_to_png.py --tmux <session> -o shot.png --ega-check`.

| endpoint | body | does |
|---|---|---|
| `POST /theme/variant` | `{"variant": "ega"\|"monochrome"}` | live switch to the Borland 1990 colours in exact 24-bit EGA (blue ░ desktop, grey menu and status line with red hotkeys, blue windows, grey dialogs with green buttons) and back. Launch in it with env `WWDOS_THEME=ega` on the app. `GET /state` reports `theme_variant` |
| `POST /windows` | `props: {colour, ink, ...}` | window colour on creation: `colour` = `blue\|cyan\|grey\|black\|magenta\|red\|green\|brown`, `ink` = 0-15 (EGA index). Primer, text_view, text_editor and figlet_text windows take both (frame and content); other windows take `blue\|cyan\|grey` as their frame. Without a colour, EGA draws these windows blue: primers and text white (15), editor and figlet yellow (14). figlet_text now gets `text`/`font` from props |
| `POST /props/{id}` | `{"props": {"colour": "cyan", "ink": 0}}` | recolour a window; `colour: "default"` clears it; 400 on an unknown colour or ink |
| `POST /menu/command` `open_primer` | `args: {path, x, y, w, h, title, colour, ink}` | primer window with a colour |
| `POST /menu/command` `open_terminal` | `args: {exec, title, hold, x, y, w, h}` | `exec`: the window runs that command (via `tools/term_exec.sh`) with no prompt, echo or cursor; `title` stays put; `hold: 1` keeps the last screen after the command ends. Returns `{"ok":true,"id":"wN"}`. Closing the window kills the command's process group |
| `GET /show/tapes` | | the film tapes (`modules/wibwob-2026/tapes/INDEX.json`, or `WWDOS_TAPES`) |
| `POST /show/film` | `tape, x?, y?, title?, actor?, lead_ms=1200, close_after=false` | a terminal window of the tape's size + 2 (centred and clamped), playing `tools/tapes/tape_play.py <tape> --at <epoch>`, epoch = now + `lead_ms`. Returns `{window_id, at, duration, fps, audio, title, cols, rows, x, y, w, h}`; `audio` is the absolute mp3 path: start it at `at` (wall clock, `time.time()`). Default leaves the last frame; `close_after` closes the window when the tape ends |
| `POST /show/nest` | `x?, y?, w?, h?, title?, theme="ega", actor?, boot_s=10` | a terminal window running a second WIBWOB-DOS (`WIBWOB_INSTANCE=nest<k>`, its stderr in `/tmp/wwdos-nest<k>.log`); waits for `/tmp/wibwob_nest<k>.sock`. Returns `{window_id, instance, depth: 1, x, y, w, h, inner: {cols, rows}}`. Binary: `WWDOS_NEST_BIN`, default `build/app/wwdos` |
| `POST /show/inner/{instance}/{verb}` | per verb | acts inside a nested desktop over its own socket. `primer` (`path` (bare name = `modules/wibwob-2026/primers/`), `x, y, w, h, title, colour, ink, frameless`), `figlet` (`text, font, x, y, w, h, colour, ink, title`), `type` (`content, cps, target=editor\|title\|figlet, id?`; no `id` opens an editor at `x, y, w, h`), `move` / `glide` (`id, x, y, w?, h?, ms`), `chrome` (`part=menu\|status\|kaomoji\|reset, text`), `nest` (opens nest<k+1> inside this one: depth 2, 3, ...), `close_all` (that desktop's windows and every nest below it), `state` |
| `GET /show/nest` · `POST /show/nest/close_all` | | list the open nests (re-adopts nests the app still runs after an API restart); end every nest, deepest first, and close their windows (also on API shutdown) |

Nesting depth on a 192x52 desktop with the default sizes (0.62 x 0.78 of the desktop, then 0.66 x 0.74 of each inner
desktop): depth 1 is 148x46 inside, depth 2 95x30, depth 3 60x18 (all three hold readable primers and figlet), depth 4
37x9 (a desktop with room for one small window), depth 5 22x5 (menu bar, status line and a strip of desktop), depth 6
20x1 (a menu bar only). Each level boots in under a second.

Stage events (same envelope as every show event):

```json
{"seq": 41, "t": 12.0, "type": "film", "actor": "wib",
 "window": {"id": "w7", "title": "the night set", "x": 55, "y": 0, "w": 82, "h": 52},
 "data": {"phase": "start", "tape": "disk-jockey", "at": 1793780739.651, "duration": 12.0, "fps": 12.0,
          "audio": "/…/modules/wibwob-2026/tapes/disk-jockey.mp3", "title": "the night set"}}
{"type": "film", "data": {"phase": "end", "tape": "disk-jockey", "at": 1793780739.651, "duration": 12.0, "closed": false}, ...}
{"type": "nest", "window": {"id": "w9", ...}, "data": {"phase": "open", "instance": "nest2", "depth": 2, "parent": "nest1"}}
{"type": "nest", "data": {"phase": "close", "instance": "nest2", "depth": 2, "parent": "nest1"}, ...}
```

### Director endpoints: batch, names, place, scene, timeline, dive

Code: `tools/api_server/show_ops.py`. Contract tests: `tools/api_server/venv/bin/python -m unittest tests/test_show_ops.py`.
Latency: `python3 tools/api_server/bench.py --api http://127.0.0.1:<port>` (p50/p95 per action; run on your own instance).

| endpoint | body | does |
|---|---|---|
| `POST /show/batch` | `{"ops":[{op...}], "atomic": false, "actor"?}` | up to 40 ops in one call. Ops on existing windows travel as one IPC `batch` message; ops that open a window go at once so later ops can use its name. State syncs once. Returns `{ok, applied, ms, results:[{i, op, ok, id?, as?, rect?:[x,y,w,h], error?}]}`. `atomic: true` checks every op first (names, place refs, primers, colours) and applies none if one fails |
| `GET /show/scene` | `?since=<seq>&minimap=true&full=false` | the scene as text rows `name\|kind\|slug\|x,y,w,h\|colour/ink\|z\|text\|id` (front first) plus a 64x18 `minimap` (each window a frame with the first letter of its name). `since` returns only windows changed after that `seq`, and `gone` (names closed since) |
| `POST /show/place` | `{"place":{...}, "w"?, "h"?, "target"?}` | resolve a place to `{rect}` without moving anything |
| `POST /show/timeline` | `{"steps":[{"t":ms,"op":{...}}], "start_ms":0, "at"?: epoch, "id"?}` | the API runs the ops itself at `at + start_ms + t` (wall clock; pass a film's `at` to lock to its music); steps with the same `t` run as one batch. Returns `{id, starts_at, ends_at, steps, applied, failed, state}`. `GET /show/timeline/{id}`, `GET /timeline/status?group_id=<id>`, `POST /timeline/cancel {"timeline_id": id}` |
| `POST /show/dive` | `{"depth":5, "gap_ms":700, "themes"?:[...], "seed"?:[slugs], "x"?, "y"?, "w"?, "h"?, "scale":0.68, "vscale":0.86}` | opens nest1, then nest2 inside it, down to `depth`, `gap_ms` apart. Themes alternate mono/EGA from the outer theme unless given; each level gets one small shelf primer (`seed`, cast first) in the strip the next level leaves free. Returns `{depth, ms, levels:[{instance, depth, theme, window_id, parent, rect, desktop:[cols,rows], seed:{slug, rect, window_id}, t}], stopped?}` |
| `POST /show/surface` | `{"gap_ms":400}` | closes every nest deepest first, one per beat. Returns `{closed:[{instance, depth, t}], ms}` |
| `POST /theme/variant` | `{"variant":"ega", "sweep_ms":600}` | `sweep_ms` rolls the new colours down the screen over that time instead of flipping at once |

Ops (every op takes `actor`; targets are a name given with `as`, a window title, or an id):
`primer {slug|path, as, place|x,y, w?, h?, title?, colour?, ink?, frameless?}` (size from the file when no w/h) ·
`figlet {text, font?, as, place|x,y, w?, h?, colour?, ink?}` · `text {content, as?, place?, w?, h?, cps?}` (opens an editor
unless `target`; `type` = text with cps 14) · `move {target, place|x,y|dx,dy, w?, h?}` · `glide {target, place|x,y, ms, ease?, wait?}`
(runs in the background unless `wait`) · `colour {target, colour?, ink?}` · `front {target}` · `close {target}` ·
`title {target, text}` · `chrome {menu?, status?, kaomoji?, reset?, cps?}` · `theme {variant, sweep_ms?}` · `wait {ms}` ·
`cursor {actor, target|x,y}` · `film {tape, as?, place|x,y, lead_ms?, close_after?}` · `nest {as?, place|x,y, w?, h?, theme?}` ·
`inner {instance (or the nest's name), verb, ...}`.

Place (`place` on any op that opens or moves a window; result rect is clamped to the desktop and slid off other
windows along the free axis unless `"avoid": false`; `dx`/`dy` nudge after):
`{"right_of":"tower","gap":2,"align":"start|center|end"}`, `left_of`, `above`, `below` · `{"on":"ledge","at":"left|center|right"}`
(rests on its top frame) · `{"inside":"wall","at":"top|bottom-left|..."}` · `{"cell":"C2"}` or `{"cell":"B2:C3"}` (6x4 grid,
columns A-F, rows 1-4; a span sets w/h when not given) · `{"anchor":"bottom-left","margin":1}` · `{"center":true}`.

IPC: the app wakes its event loop as soon as a client connects (a helper thread polls the socket), so one round trip
is about 1 ms instead of a 20 ms idle tick, and `cmd:batch lines=base64:<lines>` runs many commands in one message.
`open_primer want_id=1` answers `{"ok":true,"id":"wN"}`.

A mirror of the screen must read it with `tmux capture-pane -e -p -N`: without `-N` tmux drops the trailing blank
cells of each line, and a status line that ends in grey spaces comes back black.

Capture reads the screen with `tmux capture-pane` when `WIBWOB_CAPTURE_TMUX=<session>` is set on the API (the app runs
in that tmux session), else with the app's `/screenshot` file. Set it: the screenshot path shares `logs/screenshots/`
with every instance and names files by the second.

### Show events

Every action, from the show routes and from the classic routes (`window.created` → `open`, `window.updated` → `move`,
`window.closed` → `close`, `text.sent` → `type`, `figlet.sent` → `figlet`, `/menu/command open_primer` → `open`):

```json
{"seq": 812, "t": 41.207, "type": "land", "actor": "wib",
 "window": {"id": "w14", "title": "kitchen", "x": 52, "y": 30, "w": 36, "h": 7},
 "data": {"order": 3, "n": 9, "impact": 21.4, "col": 52, "row": 36, "col_norm": 0.329, "row_norm": 0.857}}
```

- `seq` increases by one per event; `t` is seconds since the API started. Desktop coordinates: `x` columns, `y` rows
  below the menu bar.
- `type` and its `data`:
  - `open` (`shape, cell:[c,r], index, n` when from a shape; `source:"api"` from classic routes)
  - `move` (`phase: start|step|end`; start carries `to, ms`)
  - `land` (fall: `order, n, impact` cells/s, `col, row`, `col_norm, row_norm` 0..1; `phase:"bounce"` for a bounce)
  - `fall` (`phase: start|step|end`, `ids` / `moved` / `landed`)
  - `type` (`char, i, n, target`)
  - `cursor` (`x, y, phase: step|end`)
  - `droste` (`level, depth, last?`)
  - `shape` (`ids, cell:[w,h], origin:[x,y], mask_size`)
  - `chrome` (`part: menu|status|kaomoji|reset|paint_bare, text`)
  - `close`, `figlet`, `layout`, `mark` (`label, text`), `capture` (`phase: start|stop`).
- Sonify: `land` and `open` carry position for pitch (`col_norm`, `cell`) and `order` for sequence; one key, and
  the pile ends on a chord if `order` walks a chord's degrees. Draw two cursors from `cursor` events per `actor`.

### C++ commands used by the show layer (build since the show layer)

- `move_many` IPC: `moves="id,x,y[,w,h];..."` in one round trip (a single IPC call costs ~25 ms).
- Windows smaller than 16x6 keep their size on move and resize (cursor sprites, shape pixels); bigger windows still go
  through `locate()` as before.
- `chrome_set` (`part=menu|status|kaomoji|reset`, `text`) and `paint_bare` (`id`, `on`) via `/menu/command`.
- `window_title` no longer crashes on a frameless window.

## MCP Integration

When MCP is available (`pip install fastapi-mcp`), these tools are exposed for AI agents:

- `tui_get_state` — Get current TUI state (maps to `/state`)
- `tui_create_window` — Create windows (maps to `/windows`)
- `tui_move_window` — Move/resize windows (maps to `/windows/{id}/move`)
- `tui_focus_window` — Focus windows (maps to `/windows/{id}/focus`)
- `tui_close_window` — Close windows (maps to `/windows/{id}/close`)
- `tui_cascade_windows` — Cascade layout (maps to `/windows/cascade`)
- `tui_tile_windows` — Tile layout (maps to `/windows/tile`)
- `tui_close_all_windows` — Close all windows (maps to `/windows/close_all`)
- `tui_set_pattern_mode` — Set pattern mode (maps to `/pattern_mode`)
- `tui_screenshot` — Take screenshots (maps to `/screenshot`)
- `tui_send_text` — Send text to text editor (maps to `/windows/{id}/send_text`)
- `tui_send_figlet` — Generate and send FIGlet ASCII art (maps to `/windows/{id}/send_figlet`)
- `tui_batch_layout` — Batch window operations (maps to `/windows/batch_layout`)
- `tui_timeline_cancel` — Cancel timeline operations 
- `tui_timeline_status` — Check timeline status

### MCP Example Usage (Claude Code)
```bash
# Via Claude Code MCP integration
claude -p --mcp-config=.mcp.json "Create a 4x3 grid of gradient windows with 2-pixel gaps"
claude -p --mcp-config=.mcp.json "Send HELLO WORLD in bubble font to a text editor"
claude -p --mcp-config=.mcp.json "Load the monster emoji primer file"
```

Design Notes and Next Steps
- Localhost-only, no authentication (v1 scope). API keys and remote binding come in v2.
- The `Controller` is the swap point for a C++ bridge:
  - Option A: In-process HTTP server embedded into the TV app and route directly to UI-thread handlers.
  - Option B: IPC (Unix domain socket / named pipe) where Python speaks JSON-RPC to the app; `Controller` forwards calls and mirrors state.
- Once bridged, endpoints should call into actual menu commands and window factories from `wwdos_app.cpp` and related modules.
