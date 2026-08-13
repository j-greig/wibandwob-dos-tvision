# wwdos runbook — launch, drive, debug

Operational knowledge for the TVision app (`build/app/wwdos`). Everything here was
learned the hard way; keep it current when you change the surfaces it describes.
(For the bun TS app's control API, see `~/Repos/wibandwob-dos/docs/remote-control.md`.)

## Build & launch

```sh
cmake --build build --target wwdos -j8          # main app
./scripts/launch_wwdos_ghostty.sh               # never launch bare in Ghostty — see below
```

**Always launch via `scripts/launch_wwdos_ghostty.sh`** (or replicate what it does):
Ghostty's AppleScript surface runs commands through `/usr/bin/login --noprofile --norc`
→ bare PATH → the app can't find `claude` (auth probe fails → `LLM OFF`) or `node`
(SDK bridge dead). The wrapper sets PATH, **unsets `ANTHROPIC_API_KEY`** (auth policy:
claude login only), cds to repo root, execs wwdos. Status bar shows `LLM AUTH` when
claude-login auth detected (`AuthConfig::detect()` shells out to `claude auth status`).

Window sizing to ~90% of a 2560×1440 display, via System Events after launch:

```applescript
tell application "System Events" to tell process "Ghostty"
    set position of front window to {128, 72}
    set size of front window to {2304, 1296}
end tell
```

## Control API (port 8089)

`./start_api_server.sh` — FastAPI bridge to the app's Unix socket (`/tmp/wwdos.sock`,
or `/tmp/wibwob_$WIBWOB_INSTANCE.sock`). Full endpoint list: `tools/api_server/README.md`.

**High-value surfaces:**

| Want | Use |
|---|---|
| Open a primer | `POST /menu/command {"command":"open_primer","args":{"path":"name.txt"}}` |
| Arrange art windows | `POST /gallery/arrange` — algorithms: cluster/poetry/masonry/packery/… **Prefer over `tile`** (asymmetric, art-fitted) |
| Open chat | `POST /menu/command {"command":"open_wibwob"}` |
| **Send chat message** | `POST /menu/command {"command":"wibwob_ask","args":{"text":"..."}}` |
| Scramble | commands `scramble_say`, `chat_receive`, `open_scramble` |

**Gotchas:**
- `text_view` windows render **transparent** (invisible on empty desktop) — use
  `open_primer` (frame_player) or `ascii` types for art. Zilla: avoid transparent editor.
- The generic `ascii` window type ignores `props.path` — it plays its own animation.
- **Workspaces round-trip fully** (2026-08-12): skin, desktop UTF-8 texture +
  RGB colours, per-window bg/fg overrides, shader name, frameless flags all
  serialise and restore. `save_workspace` takes optional `path` param.
  Historic saves with `preset:"custom"` used to restore nothing (dead
  branch) — fixed, old files load with explicit fields. Loader passes ALL
  props keys to registry spawns, so parameterised types survive. Known
  remaining gaps (agent audit 2026-08-12): editor/browser/paint/game inner
  state not serialised; scramble display mode saved nowhere; theme_mode is a
  no-op; registry tests still bit-rotted (52 stubs, fix list in audit).
- **After relaunching wwdos, the API server's cached socket may be stale.** It
  re-discovers on next failure, but a command sent into the gap can silently no-op.
  Verify with `GET /state` (check `windows` matches reality) before trusting a batch.
- Escape keypress in the chat window **cancels the in-flight LLM request**.
- `command_registry_test` / `scramble_engine_test` are **bit-rotted**: their stub
  files lag the registry by ~30 api_* externs (missing figlet/paint/spawn stubs,
  pre-existing). Fixing means regenerating the stub list, not adding one stub.
- Windows spawned with no explicit rect use `TWwdosApp::findSpreadRect()` (least-overlap
  placement, added 2026-08). If new spawn paths hardcode rects, route them through it —
  jumbled/obscured windows are a bug, not a vibe.
- **Theming canon**: views take colours from `ThemeManager::attr(SkinRole)`
  (Paper/Dialog/Bar/Dim/Accent/Floor/Frame*/Ok/Warn...), never hand-rolled
  TColorRGB/0x07 attrs — see docs/development/theming-roles.md for the role
  vocabulary, derivations and the content-vs-affordance rule. New skins are
  one kSkins row; every role derives.
- **Skins are hot files**: `skins/*.skin` (key=value, spec in skins/README.md)
  load at boot and shadow built-ins by name; `set_skin` auto-reloads on
  unknown names so a freshly written file applies immediately. `list_skins`
  (JSON registry), `reload_skins` (re-read + re-apply active, also View →
  Skins → Reload Skin Files), `skin_save name=x` (persist a skin to file).
  A .skin file can remap all 16 terminal palette slots (pal0..pal15) — a
  whole monitor in a text file. Starter pack: vaporwave, gameboy, amber-crt,
  bloodmoon, seafoam, c64.
- **Relaunches auto-save**: scripts/relaunch_wwdos.sh writes
  workspaces/pre_relaunch.json before killing — never eat a composition.
- **CGA skins (one-shot)**: `set_skin` with `skin` param — `dflat` (D-Flat MemoPad:
  blue ▒ sea, grey paper, blue dialogs), `turbo` (Turbo Pascal), `terra` (GeoGraphics),
  `pipeline` (black/blue/magenta), `off` restores house grey. The active skin persists across relaunches
  via `.wwdos_skin` (written by set_skin, read at boot, gitignored).
  Applies chrome + desktop
  (authentic CGA **RGB**, immune to terminal palette remapping) + papers all
  primer/text windows. Recipes: `theme_manager.cpp` kSkins (single source); refs in
  `design/figma-refs/`. Skins colour **primer/text windows only** — generative app
  windows keep native palettes (deliberate, per Zilla). Dialog accents stay
  per-window `set_window_bg`/`set_window_fg` (CGA 0-15). Verify via `/state`
  `skin` + `cga_chrome` fields — a set_skin sent in the same burst as window-opens
  can no-op (socket race); check and resend.
- **Disk Library launcher**: `open_disks` (API) or View → Disk Library — the
  SYMBIENT SHAREWARE LIBRARY. Each 3.5" floppy = one registry command; Enter or
  double-click boots it (arrows/Home/End navigate, scrollbar for overflow).
  Catalogue lives in `TDiskLibraryWindow::populateDisks()`
  (disk_library_view.cpp): title, label art lines, CGA body/label colours,
  command + args. Skin disks (DFLAT 1/8 … PIPELINE 4/8) reskin the desktop on
  boot. Views can execute any registry command in-process via
  `wwdos_exec_command(name, kv)` (command_registry.h). Optional
  `disk_library.cat` in repo root overrides the built-in catalogue —
  copy `disk_library.cat.example` and edit (format documented in both;
  gitignored, no rebuild, reopen the window to reload).
- **Relaunching wwdos**: use `./scripts/relaunch_wwdos.sh` — kills the old
  process, closes its Ghostty window (tracked in /tmp/wwdos_ghostty_win),
  spawns fresh with `wait after command` OFF so dead surfaces auto-close.
  Never leave stray "Process exited" terminals behind.
- **SHADER.SYS — the ASCII shader host** (tweet_shader_view.cpp)

  Open it:
  - API: `POST /menu/command {"command":"open_shader","args":{"shader":"wibrain"}}`
    (omit `shader` for the default, isotower; also accepts x/y/w/h via the
    `shader` window type)
  - Menu: View → Mono Shader (Generative)
  - Library: boot the SHADER.SYS disk (opens isotower)

  Drive it (keys, window focused):
  - `N` / Tab — next shader · `P` — phosphor (white/green/amber/cyan) ·
    space — pause/resume. Active shader name shows bottom-left.

  Registered shaders: `isotower` (painter-algorithm iso voxel city),
  `wibrain` (kaomoji rain), `beastiemelt` (liquefying beastie portrait),
  `plasma` (sin-interference spelt in ~wobWOB*o0), `wallsofcode` (text raycaster
  after @KilledByAPixel dwitter 35982 — walls typeset from its own source),
  `yohei-rocks` (tsubuyaki-GLSL port, credited), `tunnel` (square flythrough).

  Add a shader (one entry in kShaders, pick ONE contract):
  - `float fn(u, v, t)` — per-pixel luminance 0..1 (u,v square-normalised)
  - `void frame(W, H, t, float* lum)` — full-frame luminance
  - `void frameG(W, H, t, float* lum, char* glyphs)` — luminance + your own
    ASCII glyph per cell (0 = fall back to the ramp " .:-=+*#%@")
  Then add the name to the open_shader capability string in
  command_registry.cpp. Luminance <0.3 renders dim grey, ≥0.3 phosphor.

  Porting lessons (learned porting yohei-rocks): GLSL `vec*mat3` is a
  ROW-vector multiply (rotation by -angle); float overflow must be clamped
  like GPUs do silently; prototype in numpy against a reference frame
  BEFORE entering the C++ build loop — 10x faster iteration. And for
  isometric anything, painter-algorithm projection beats raymarching
  repeated SDFs (tile-local fields tunnel across cell borders).
- **Screensaver**: idle 10min → fullscreen random SHADER.SYS resident; any
  key/mouse wakes (the waking event is swallowed). `screensaver` command:
  `action` now|on|off, `minutes` sets the timeout (0 disables). IPC
  commands do NOT reset the idle clock — only real input does.
- **Dense skinned scene**: `./scripts/skin_scene.sh [skin]` — 7 overlapping primer
  windows scaled to the live canvas + skin + dialog accents. Matches the Figma-ref
  density; two windows on a sea is a haiku, the refs are a pub argument.
- Legacy manual route still works: `set_theme_variant cga` + `desktop_texture ▒` +
  `desktop_color fg,bg` (RGB-mapped when CGA chrome on) + per-window colours.
- `/gallery/arrange` art-installation mode: `frameless+shadowless+padding:0` = chromeless
  glyph blocks. `stamp` pattern `text` spells words in primer-windows (3×5 pixel font,
  one window per lit pixel — magnificent with tiny primers like cave-monster 9×3).
  Caveat: windows clamp to ~20×5 minimum, so sub-minimum primers carry black padding.

## LLM plumbing (chat)

Flow: `wibwob_view` → `WibWobEngine` → `ClaudeCodeSDKProvider` (C++) → spawns
`node app/llm/sdk_bridge/claude_sdk_bridge.js` → `@anthropic-ai/claude-agent-sdk` → claude CLI auth.

- **Model resolution has ONE home**: `resolveModelId()` in
  `app/llm/providers/claude_code_sdk_provider.cpp` (full `claude-*` ids pass
  verbatim; bare aliases map there). Config: `app/llm/config/llm_config.json`;
  the JS bridge is pure passthrough. Current: `claude-sonnet-5`. History note:
  it used to be clamped in three places — never reintroduce mapping elsewhere.
- **`app/llm/sdk_bridge` needs `npm install`** — a fresh clone has no node_modules and
  the bridge dies on `require('@anthropic-ai/claude-agent-sdk')`; the chat then hangs
  at "Wobbling..." forever with no visible error. Check first when chat is dead.
- System prompt: `modules-private/wibwob-prompts/wibandwob.prompt.md` (first hit in the
  search list in `wibwob_view.cpp`). Missing file → fallback prompt (single-voice, bland).
- Chat transcripts: `logs/chat_*.log`. Agent SDK session jsonl (proves actual model):
  `~/.claude/projects/-Users-james-Repos-wibandwob-dos-tvision/*.jsonl` → grep `"model"`.
- Verify auth/model state in-app: Help → LLM Status; status-bar `LLM AUTH|KEY|OFF`.
- In-app Wib&Wob drive the desktop via MCP tools (`mcp_tools.js` → axios → :8089,
  30s timeout). If they claim "the desktop went dark", check `GET /state` first —
  it's usually one timed-out call, not a dead connection. Bridge code changes only
  take effect in newly opened chat windows (the node process is per-session).

## Event-loop wake (fixed 2026-08, keep it working)

Two distinct stalls, both fixed in `ApiIpcServer` — if API-driven changes ever stop
appearing until a keypress, one of these regressed:

1. **Starvation**: TVision's `getEvent` blocks on terminal input; `idle()` (which
   pumps the IPC server and Scramble) never runs. Fix: watcher thread `select()`s
   the listen fd and calls `TEventQueue::wakeUp()` when a connection is pending.
2. **Missing final flush**: the screen flushes at the TOP of the event cycle, before
   `idle()` — so the last command of a burst paints into the buffer and stays
   invisible (earlier commands get flushed by the next command's wake). Fix: the
   watcher thread fires a **trailing** cross-thread `wakeUp()` ~50ms after each
   pending-connection wake. (A same-thread `wakeUp()` inside `poll()` races and can
   be swallowed — don't rely on it.)

Symptom key: IPC log all ✓ + `get_state` correct + screen stale + `API IDLE` in the
status bar = flush problem, not a dead app. F5 repaint reveals everything.

FIXED (2026-08-11), two layers: (a) `handleClient` now calls
`TScreen::flushScreen()` synchronously after every command (it runs on the main
thread from idle(), so the just-drawn buffer goes straight out); (b) the watcher
thread keeps firing trailing wakes until the command stream has been quiet for
~250ms (closes the large-burst tail). No more F5 after API bursts.

**Screenshot verification trap (macOS App Nap)**: a BACKGROUNDED Ghostty window
gets its rendering throttled — `screencapture -l<id>` then returns the stale
backing store, which looks exactly like a flush failure (state correct, pixels
frozen). It is not one. `osascript -e 'tell application "Ghostty" to activate'`
+ ~1s before every screenshot. Hours were lost to this; do not re-diagnose it.

## Ghostty AppleScript (launching & driving any TUI binary)

Ghostty ships an AppleScript dictionary (v1.3+): `sdef /Applications/Ghostty.app`,
docs at https://ghostty.org/docs/features/applescript

```applescript
tell application "Ghostty"
    set cfg to new surface configuration
    set command of cfg to "/absolute/path/to/binary"   -- or the launch wrapper
    set wait after command of cfg to true
    set w to new window with configuration cfg
    activate window w
end tell
```

Gotchas (2026-08):
- **PATH is bare** — `command` runs via `/usr/bin/login --noprofile --norc`. Absolute
  paths only; for wwdos use `scripts/launch_wwdos_ghostty.sh` (PATH + auth policy).
- **No bounds control in AppleScript** — resize via System Events on the frontmost
  window after ~2s (`set position` / `set size`).
- `input text` / `send key` only compile inside `tell application "Ghostty"`.
- `new window`, never `make new window` (error -2710 by design).
- Delay ~0.5–1.5s after window creation before `input text`.
- `send key` is flaky for printables — `input text` for text, `send key` for
  `"enter"`/modifier chords. **Escape in the chat window cancels the LLM request.**
- `perform action "toggle_fullscreen"` / `"decrease_font_size:2"` etc — uses `on`, not `to`.
- Surface config props: `command`, `initial working directory`, `initial input`,
  `wait after command`, `font size`. Driving: `input text ... to t`, `send key`,
  `send mouse button/position/scroll`, tabs via `new tab in w`.

## Screenshot / verify loop

```sh
# find the window id, capture without focusing
python3 -c "import Quartz; [print(w['kCGWindowNumber']) for w in Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionOnScreenOnly, Quartz.kCGNullWindowID) if w.get('kCGWindowOwnerName')=='Ghostty']"
screencapture -x -o -l<ID> /tmp/shot.png
```

Screenshot after every batch of API calls — `ok:true` from the API does not mean
pixels changed (see socket-race gotcha above).
