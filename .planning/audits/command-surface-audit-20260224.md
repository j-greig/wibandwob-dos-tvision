# Command Surface Audit — WibWob-DOS

**Date:** 2026-02-24
**Author:** Codex analyst + Claude Code
**Scope:** All command/control surfaces, discovery paths, parity gaps, and bugs

## TL;DR

1. **C++ command registry is the real source of truth** (57 commands with descriptions), but the API/MCP layers currently lose or hide that metadata in multiple places.
2. **Biggest live failure: IPC auth mismatch** — C++ can require HMAC auth, but `ipc_client.py` doesn't implement the handshake, so capability discovery silently degrades to empty command lists (`/commands` and `/capabilities` return `commands: []`).
3. **`/capabilities` intentionally strips descriptions/params** down to names only — and when IPC fails, even names are empty. Agents are flying blind.
4. **Python window type enum is stale** — 7 spawnable C++ types (`quadra`, `snake`, `rogue`, `deep_signal`, `app_launcher`, `gallery`, `figlet_text`) are rejected by `POST /windows`.
5. **No machine-readable parameter schemas exist anywhere** — agents must guess param names from free-text descriptions or trial and error.

## Architecture Diagram

```
                                ┌─────────────────────────────┐
                                │      Keyboard / Menus       │
                                │   app/wwdos_app.cpp         │
                                │ (cm* handlers, direct calls)│
                                └──────────────┬──────────────┘
                                               │ mostly bypasses registry
                                               v
┌─────────────────────────────┐      ┌─────────────────────────────┐
│ Embedded Claude SDK Bridge  │─────▶│ Node MCP tools (2 tools)    │
│ claude_sdk_bridge.js        │      │ mcp_tools.js                │
│ - injects /capabilities     │      │ - tui_list_commands → GET /commands
│ - refs nonexistent tool     │      │ - tui_menu_command → POST /menu/command
└──────────────┬──────────────┘      └──────────────┬──────────────┘
               │ HTTP                                │ HTTP
               v                                     v
        ┌─────────────────────────────────────────────────────────┐
        │        FastAPI server (tools/api_server/main.py)        │
        │                                                         │
        │  Direct REST endpoints          Registry gateway        │
        │  /windows, /paint/*, ...        POST /menu/command      │
        │  (many bypass registry)         (ctl.exec_command)      │
        │                                                         │
        │  Discovery endpoints            FastAPI MCP (/mcp)      │
        │  GET /commands, /capabilities   (auto-exposes routes)   │
        └──────────────┬──────────────────────────────┬───────────┘
                       │                              │
                       │ controller.py                │ route schemas
                       v                              │
             ┌──────────────────────┐                 │
             │ ApiController        │◀────────────────┘
             │ - get_registry_caps  │
             │ - get_window_types   │
             │ - exec_command       │
             │ - lots of direct IPC │
             └──────────┬──────────┘
                        │ send_cmd()
                        v
             ┌─────────────────────────────┐
             │ Unix socket IPC client      │
             │ ipc_client.py               │
             │ line protocol, NO HMAC auth │  ◀── BUG: auth mismatch
             └──────────┬──────────────────┘
                        │ "cmd:<name> k=v ..." + newline
                        v
             ┌─────────────────────────────────────────────┐
             │ C++ IPC server (app/api_ipc.cpp)            │
             │ - optional HMAC challenge/auth               │
             │ - inline dispatch (create_window, move, etc) │
             │ - exec_command → registry dispatch            │
             │ - get_capabilities / get_window_types JSON    │
             └──────────┬───────────────────────┬──────────┘
                        │                       │
                        v                       v
             ┌──────────────────────┐   ┌──────────────────────────┐
             │ C++ Command Registry │   │ C++ App methods/helpers  │
             │ 57 commands + descs  │   │ TWwdosApp + api_* funcs  │
             └──────────────────────┘   └──────────────────────────┘

Where descriptions are AVAILABLE:  C++ registry JSON, GET /commands (when IPC works)
Where descriptions are LOST:       GET /capabilities, Node bridge prompt injection
Where param schemas exist:         Direct REST endpoints (Pydantic models)
Where param schemas are MISSING:   /menu/command, Node MCP, IPC direct
```

## Known Bugs & Gaps

### P0 — Critical (blocks agent usability)

**1. IPC auth handshake mismatch → empty command discovery**
- C++ IPC server supports optional HMAC (`WIBWOB_AUTH_SECRET`) and returns `{"error":"auth_failed"}` on failure
- Python `ipc_client.py` sends commands immediately with no auth handshake
- Controller swallows error JSON via `except Exception: pass` → falls back to `{"commands": []}`
- **Result:** `GET /capabilities` and `GET /commands` return empty commands
- Files: `api_ipc.cpp:178-364`, `ipc_client.py:26-85`, `controller.py:102-112`
- Fix: implement HMAC in `ipc_client.py` OR disable auth for local; fail loudly on `{"error":...}`

**2. `/capabilities` strips command descriptions by design**
- `controller.py:136-146` extracts only `cmd.get("name")` from registry
- `Capabilities` schema only allows `commands: List[str]`
- Embedded bridge injects this into system prompt → agent sees names only, no descriptions
- Files: `controller.py:130-146`, `schemas.py:185-188`, `claude_sdk_bridge.js:199-201`
- Fix: expand `Capabilities.commands` to include descriptions + params

**3. Silent exception swallowing in capability fetchers**
- `controller.py:110` — bare `except Exception: pass` on IPC failure
- No logging, no degraded-mode indicator
- Fix: reject `{"error":...}` responses explicitly; add `registry_source: live|fallback` to response

### P1 — High (broken features / parity drift)

**4. `/paint/line` and `/paint/rect` routes pass wrong params to controller**
- REST routes pass `fg/bg/fill` but controller expects only `erase`
- Result: `TypeError` at runtime — endpoints are broken
- Files: `main.py:403-411`, `controller.py:446-449`

**5. Python window type enum is stale — 7 types rejected by `POST /windows`**
- Missing: `quadra`, `snake`, `rogue`, `deep_signal`, `app_launcher`, `gallery`, `figlet_text`
- Extra in Python: `wallpaper` (not in C++)
- Files: `models.py:10-37`, `schemas.py:15-18`, `window_type_registry.cpp:334-340`

**6. Embedded bridge references nonexistent `tui_create_window` tool**
- Node MCP only has `tui_list_commands` and `tui_menu_command`
- Bridge prompt text references `tui_create_window` → agent hallucinates
- Files: `claude_sdk_bridge.js:199-201`, `mcp_tools.js:6-67`

**7. `CommandCapability.requires_path` is semantically wrong**
- Named `requires_path` but means "has required args" — used for `wibwob_ask`, `set_theme_mode`, etc.
- No actual parameter schema exists anywhere
- Files: `command_registry.h:9-12`

### P2 — Medium

**8. IPC request framing is fragile (single 2048-byte `read()`)**
- `api_ipc.cpp:372-374` — assumes full command arrives in one read
- Long payloads (text injection, base64) can truncate
- Fix: read until newline

**9. Dangerous commands are under-described for agent safety**
- `close_all`: "Close all windows" — no warning about irreversibility
- `inject_command`: exposed in registry with minimal description
- `/windows/close_all` has `operation_id="canvas_clear"` which obscures destructive semantics
- No `dangerous`/`destructive` metadata field exists

**10. TUI menu actions bypass command registry**
- Menu handlers call direct app methods (`cascade()`, `tile()`, etc.)
- Behavior can drift between menu, REST, and `/menu/command`
- Files: `wwdos_app.cpp:1140-1691`

**11. Cross-surface parameter naming inconsistency**
- Registry uses `id` for paint commands; REST uses `win_id`
- Same command, different param names depending on entry point

## Full Command Registry (57 commands)

| Command | Description | Params | REST | Node MCP | Menu |
|---------|-------------|--------|------|----------|------|
| `cascade` | Cascade all windows on desktop | — | ✅ | ✅* | ✅ |
| `tile` | Tile all windows on desktop | — | ✅ | ✅* | ✅ |
| `close_all` | Close all windows | — | ✅ | ✅* | ✅ |
| `save_workspace` | Save current workspace | — | ✅ | ✅* | ✅ |
| `open_workspace` | Open workspace from a path | path | ✅ | ✅* | ✅ |
| `screenshot` | Capture screen to a text snapshot | — | ✅ | ✅* | ✅ |
| `pattern_mode` | Set pattern mode: continuous or tiled | mode | ✅ | ✅* | ✅ |
| `set_theme_mode` | Set theme mode: light or dark | mode | ✅ | ✅* | — |
| `set_theme_variant` | Set theme variant | variant | ✅ | ✅* | — |
| `reset_theme` | Reset theme to default | — | ✅ | ✅* | — |
| `open_scramble` | Toggle Scramble cat overlay | — | ✅ | ✅* | ✅ |
| `scramble_expand` | Toggle Scramble smol/tall | — | ✅ | ✅* | ✅ |
| `scramble_say` | Message Scramble chat | text | ✅ | ✅* | — |
| `scramble_pet` | Pet the cat | — | ✅ | ✅* | — |
| `open_room_chat` | Open Room Chat window | — | ✅ | ✅* | ✅ |
| `room_chat_receive` | Deliver chat msg to RoomChatView | sender, text, ts | ✅ | ✅* | — |
| `room_presence` | Update participant list | participants | ✅ | ✅* | — |
| `new_paint_canvas` | Open paint canvas | — | ✅ | ✅* | ✅ |
| `open_micropolis_ascii` | Open Micropolis ASCII | — | ✅ | ✅* | ✅ |
| `open_quadra` | Open Quadra blocks game | — | ✅ | ✅* | ✅ |
| `open_snake` | Open Snake game | — | ✅ | ✅* | ✅ |
| `open_rogue` | Open Rogue dungeon crawler | — | ✅ | ✅* | ✅ |
| `open_deep_signal` | Open Deep Signal scanner | — | ✅ | ✅* | ✅ |
| `open_apps` | Open Applications browser | — | ✅ | ✅* | ✅ |
| `open_gallery` | Open ASCII Art Gallery | — | ✅ | ✅* | ✅ |
| `gallery_list` | List primer filenames | tab, search | ✅ | ✅* | — |
| `open_primer` | Open primer by name | path, x, y, w, h, frameless, shadowless, title | ✅ | ✅* | — |
| `open_terminal` | Open terminal emulator | — | ✅ | ✅* | ✅ |
| `terminal_write` | Send text to terminal | text, window_id | ✅ | ✅* | — |
| `terminal_read` | Read terminal content | window_id | ✅ | ✅* | — |
| `chat_receive` | Remote chat msg to Scramble | sender, text | ✅ | ✅* | — |
| `wibwob_ask` | Send msg to Wib&Wob chat | text | ✅ | ✅* | — |
| `get_chat_history` | Get chat history JSON | — | ✅ | ✅* | — |
| `paint_cell` | Set canvas cell | id, x, y, fg, bg | ✅ | ✅* | — |
| `paint_text` | Write text on canvas | id, x, y, text, fg, bg | ✅ | ✅* | — |
| `paint_line` | Draw line on canvas | id, x0, y0, x1, y1, erase | ✅ | ✅* | — |
| `paint_rect` | Draw rectangle on canvas | id, x0, y0, x1, y1, erase | ✅ | ✅* | — |
| `paint_clear` | Clear canvas | id | ✅ | ✅* | — |
| `paint_export` | Export canvas as text | id | ✅ | ✅* | — |
| `paint_save` | Save canvas to .wwp | id, path | ✅ | ✅* | — |
| `paint_load` | Load canvas from .wwp | id, path | ✅ | ✅* | — |
| `open_paint_file` | Open paint with .wwp loaded | path | ✅ | ✅* | — |
| `paint_stamp_figlet` | Stamp FIGlet on canvas | id, text, font, x, y, fg, bg | ✅ | ✅* | — |
| `list_figlet_fonts` | List FIGlet fonts | — | ✅ | ✅* | — |
| `preview_figlet` | Render FIGlet text | text, font, width | ✅ | ✅* | — |
| `inject_command` | Inject raw IPC command ⚠️ | cmd_id | ✅ | ✅* | — |
| `window_shadow` | Toggle window shadow | id, on | ✅ | ✅* | — |
| `window_title` | Set window title | id, title | ✅ | ✅* | — |
| `desktop_preset` | Set desktop preset | preset | ✅ | ✅* | — |
| `desktop_texture` | Set desktop fill char | char | ✅ | ✅* | — |
| `desktop_color` | Set desktop fg/bg colour | fg, bg | ✅ | ✅* | — |
| `desktop_gallery` | Toggle gallery mode | on | ✅ | ✅* | — |
| `desktop_get` | Get desktop state | — | ✅ | ✅* | — |
| `figlet_set_text` | Change figlet window text | id, text | ✅ | ✅* | — |
| `figlet_set_font` | Change figlet window font | id, font | ✅ | ✅* | — |
| `figlet_set_color` | Set figlet colours | id, fg, bg | ✅ | ✅* | — |
| `figlet_list_fonts` | List figlet font names | — | ✅ | ✅* | — |

`✅*` = reachable via `tui_menu_command` but discovery fails when `/commands` returns empty (IPC auth bug)

## Window Type Parity

| Type | C++ spawnable | Python enum | `POST /windows` | Notes |
|------|:---:|:---:|:---:|-------|
| `test_pattern` | ✅ | ✅ | ✅ | Protocol slug (kept) |
| `gradient` | ✅ | ✅ | ✅ | |
| `frame_player` | ✅ | ✅ | ✅ | |
| `text_view` | ✅ | ✅ | ✅ | |
| `text_editor` | ✅ | ✅ | ✅ | |
| `browser` | ✅ | ✅ | ✅ | |
| `verse` | ✅ | ✅ | ✅ | |
| `mycelium` | ✅ | ✅ | ✅ | |
| `orbit` | ✅ | ✅ | ✅ | |
| `torus` | ✅ | ✅ | ✅ | |
| `cube` | ✅ | ✅ | ✅ | |
| `life` | ✅ | ✅ | ✅ | |
| `blocks` | ✅ | ✅ | ✅ | |
| `score` | ✅ | ✅ | ✅ | |
| `ascii` | ✅ | ✅ | ✅ | |
| `animated_gradient` | ✅ | ✅ | ✅ | |
| `monster_cam` | ✅ | ✅ | ✅ | |
| `monster_verse` | ✅ | ✅ | ✅ | |
| `monster_portal` | ✅ | ✅ | ✅ | |
| `paint` | ✅ | ✅ | ✅ | |
| `micropolis_ascii` | ✅ | ✅ | ✅ | |
| `terminal` | ✅ | ✅ | ✅ | |
| `room_chat` | ✅ | ✅ | ✅ | |
| `wibwob` | ✅ | ✅ | ✅ | Comment says internal-only but has spawn callback |
| `scramble` | ❌ | ✅ | ❌ | State-only, not spawnable |
| **`quadra`** | ✅ | ❌ | ❌ | **Missing from Python** |
| **`snake`** | ✅ | ❌ | ❌ | **Missing from Python** |
| **`rogue`** | ✅ | ❌ | ❌ | **Missing from Python** |
| **`deep_signal`** | ✅ | ❌ | ❌ | **Missing from Python** |
| **`app_launcher`** | ✅ | ❌ | ❌ | **Missing from Python** |
| **`gallery`** | ✅ | ❌ | ❌ | **Missing from Python** |
| **`figlet_text`** | ✅ | ❌ | ❌ | **Missing from Python** |
| `wallpaper` | ❌ | ✅ | ❌ | **Python-only, not in C++** |

## Recommendations (priority ordered)

### P0 — Do now
1. Fix IPC auth compatibility in `ipc_client.py` — implement HMAC or disable; fail loudly on `{"error":...}`
2. Stop stripping command metadata in `/capabilities` — include descriptions at minimum
3. Fix silent exception swallowing in `controller.py` discovery methods

### P1 — Do this week
4. Fix `/paint/line` and `/paint/rect` broken param signatures
5. Update Python `WindowType` enum + `WindowCreate` schema to match C++ (add 7 types, remove `wallpaper`)
6. Fix embedded bridge stale tool reference (`tui_create_window` → `tui_menu_command`)
7. Add structured `params` metadata to C++ `CommandCapability` struct

### P2 — Do this sprint
8. Add `dangerous: bool` metadata to registry commands
9. Fix IPC read framing (read until newline, not single 2048-byte read)
10. Standardise param names across surfaces (`id` vs `win_id`)
11. Route menu actions through command registry where practical
