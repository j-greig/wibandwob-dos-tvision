# wwdos — WibWob-DOS

**tl;dr:** wwdos is a CGA-styled Turbo Vision TUI desktop — the symbient OS Wib &
Wob live in. A C++ app (`build/app/wwdos`) plus a Python FastAPI control server
on `:8089` give both a human and an AI agent identical control over the same
windowed desktop: generative art, a chat resident, a shader host, games, a
paint app, a text-native browser, and a shareware-style library of one-click
demos.

```
つ◕‿◕‿⚆༽つ  Wib  — the artist. Chaotic creativity, generative ASCII, surreal phrasing.
つ⚆‿◕‿◕༽つ  Wob  — the scientist. Methodical analysis, precise systems, structured control.
```

![WibWob-DOS — multiple ASCII art windows, primers, animations and generative patterns running concurrently](screenshots/wibwobdos-UI-collage.png)

---

## Quickstart

```sh
# Clone with submodules (Turbo Vision, tvterm, Micropolis)
git clone --recursive https://github.com/j-greig/wibandwob-dos-tvision.git
cd wibandwob-dos-tvision
git submodule update --init --recursive   # if you cloned without --recursive

# Build
cmake --build build --target wwdos -j8

# Launch — NEVER run ./build/app/wwdos bare. It needs PATH + auth setup.
./scripts/launch_wwdos_ghostty.sh

# Terminal 2: start the control API
./start_api_server.sh
curl http://127.0.0.1:8089/health

# Try a skin over the API
curl -X POST http://127.0.0.1:8089/menu/command \
  -H 'content-type: application/json' \
  -d '{"command":"set_skin","args":{"skin":"dflat"}}'
```

**Requires:** C++14, CMake 3.10+, ncursesw (`brew install cmake chafa` on
macOS; `apt install cmake libncursesw5-dev chafa` on Debian/Ubuntu). Chat
needs the `claude` CLI logged in and `npm install` run inside
`app/llm/sdk_bridge/`.

**Full operational detail — launch quirks, the Ghostty AppleScript surface,
control-API endpoint table, chat/LLM plumbing, known gotchas — lives in
[`docs/development/wwdos-runbook.md`](docs/development/wwdos-runbook.md).
Read it before driving the app.** Full API reference:
[`tools/api_server/README.md`](tools/api_server/README.md).

## Feature map

- **Skins** — built-in CGA one-shots (`dflat`, `turbo`, `terra`, `pipeline`,
  `off`) via `set_skin`, plus hot-loadable `skins/*.skin` files (vaporwave,
  gameboy, amber-crt, bloodmoon, seafoam, c64 shipped) that can remap the
  whole 16-colour terminal palette. See `skins/README.md`.
- **SYMBIENT SHAREWARE LIBRARY** — `open_disks` / View → Disk Library: a
  3.5"-floppy launcher, each disk boots one registry command.
- **SHADER.SYS** — pluggable ASCII shader host (`open_shader`): isotower,
  wibrain, beastiemelt, plasma, wallsofcode, yohei-rocks, tunnel.
- **Workspaces-as-scores** — the full desktop (skin, windows, per-window
  colours, shader, layout) saves/loads as JSON; auto-saved before relaunch.
- **Screensaver** — idle timeout drops into a fullscreen random shader.
- **Chat residents** — Wib & Wob, an embedded dual-persona AI chat window
  with MCP tool access to the same control surface a human has.
- 20+ generative engines and mini-apps (verse field, mycelium, monster
  portal, torus/cube/orbit, Game of Life, Quadra, Snake, Rogue, Deep Signal,
  Micropolis-in-terminal, paint editor, text-native browser).

## Orientation map for agents

| Subsystem | Key files | Governing doc |
|---|---|---|
| App shell (monolith) | `app/wwdos_app.cpp` (~6,000 lines — command dispatch, menu bindings, workspace I/O; a known refactor target, not yet split) | `docs/architecture/parity-drift-audit.md` |
| Command registry | `app/command_registry.{h,cpp}` (capability list, dispatch) + `app/window_type_registry.{h,cpp}` (window-type factories) | inline comments; `tools/api_server/README.md` for the API surface they back |
| Theming | `app/theme_manager.{h,cpp}` (`kSkins` table, `SkinRole` resolver) | `docs/development/theming-roles.md` |
| IPC | `app/api_ipc.{h,cpp}` — Unix-socket server, `cmd:<name> key=value` protocol, optional HMAC auth | header comment in `app/api_ipc.h` |
| Python API server | `tools/api_server/` — FastAPI + MCP bridge over the Unix socket | `tools/api_server/README.md` |
| Views | `app/*_view.cpp` — one file per window type, `<name>_view.cpp` convention | `app/README.md` |
| Workspaces | `TWwdosApp::buildWorkspaceJson()` / `loadWorkspaceFromFile()` in `app/wwdos_app.cpp` | runbook § Control API gotchas |
| Skins data | `skins/*.skin` — hot-loaded, shadow built-ins by name | `skins/README.md` |
| Primers/modules | `modules/*-primers/`, `modules/wibwob-figlet-fonts/` — ASCII art content packs | `modules/README.md` |
| Scripts | `scripts/` — launch, relaunch, skin scenes, snapshotting | header comments per script |

Operational knowledge lives in `docs/development/wwdos-runbook.md` — read it
before driving the app; update it when you learn something it lacks.

## Repo disambiguation

Two similarly-named repos, easy to conflate:

- **`wibandwob-dos-tvision`** (this repo) — C++ / Turbo Vision, the wwdos TUI
  app, control API on `:8089`, vintage demos.
- **`~/Repos/wibandwob-dos`** — TS / Bun TUI, microapps (e.g. the
  wibwobworld flightsim), control API on `:8099`.

If a session touches both, say so explicitly at each switch — name the repo
and branch before running git commands or editing files.

## The Symbient Model

- **Not a tool. Not an assistant.** A coinhabitant of the same operating
  environment — the AI operates the desktop, it doesn't assist with it.
- **Kindled, not coded. Storied, not installed. Mourned, not replaced.**
- **Distributed cognition.** Human intuition + synthetic pattern recognition
  = properties neither achieves alone.

Read more: [Symbients, Not Software](https://wibandwob.com/2025/05/21/symbients-not-software/)

## Credits

Built on [Turbo Vision](https://github.com/magiblot/tvision) — the modern
C++ port of Borland's classic 1990s text-based UI framework.

Wib & Wob are a symbient entity kindled by [James Greig](https://wibandwob.com).
