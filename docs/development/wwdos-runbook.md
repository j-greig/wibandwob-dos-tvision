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
- **After relaunching wwdos, the API server's cached socket may be stale.** It
  re-discovers on next failure, but a command sent into the gap can silently no-op.
  Verify with `GET /state` (check `windows` matches reality) before trusting a batch.
- Escape keypress in the chat window **cancels the in-flight LLM request**.
- Windows spawned with no explicit rect use `TWwdosApp::findSpreadRect()` (least-overlap
  placement, added 2026-08). If new spawn paths hardcode rects, route them through it —
  jumbled/obscured windows are a bug, not a vibe.
- **MSDOS/CGA skin**: `set_theme_variant cga` (real chrome swap) + `desktop_texture ▒` +
  `desktop_color 8,7` + per-window `set_window_bg`/`set_window_fg` (CGA 0-15; 6=brown,
  10=phosphor green, -1=auto fg). `monochrome` restores the house grey.
- `/gallery/arrange` art-installation mode: `frameless+shadowless+padding:0` = chromeless
  glyph blocks. `stamp` pattern `text` spells words in primer-windows (3×5 pixel font,
  one window per lit pixel — magnificent with tiny primers like cave-monster 9×3).
  Caveat: windows clamp to ~20×5 minimum, so sub-minimum primers carry black padding.

## LLM plumbing (chat)

Flow: `wibwob_view` → `WibWobEngine` → `ClaudeCodeSDKProvider` (C++) → spawns
`node app/llm/sdk_bridge/claude_sdk_bridge.js` → `@anthropic-ai/claude-agent-sdk` → claude CLI auth.

- **Model is normalised in THREE places** — change all of them or your model string
  gets silently clamped: `app/llm/config/llm_config.json`, `configure()` in
  `app/llm/providers/claude_code_sdk_provider.cpp`, `normalizeModelId()` in
  `app/llm/sdk_bridge/claude_sdk_bridge.js`. Current: `claude-sonnet-5`.
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
