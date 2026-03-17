# TVision C++ Repo Cleanup Plan

> Working branch: `last-days-of-tvision`
> Goal: make `j-greig/wibandwob-dos-tvision` a clean, standalone TVision/C++ archive
> Status: **READY TO EXECUTE** — all questions answered by human

---

## Branch Strategy

**`last-days-of-tvision` becomes the new `main`.** Current `main` is useless — fully TS/Bun, no `app/` or CMake. Old `main` content is preserved in `j-greig/wibandwob-dos-backup`.

1. Do all cleanup on `last-days-of-tvision`
2. Force-push: `git push origin last-days-of-tvision:main --force`
3. Set default branch: `gh repo edit j-greig/wibandwob-dos-tvision --default-branch main`
4. Delete old remote branch: `git push origin --delete last-days-of-tvision`
5. Locally: `git checkout main && git branch -D last-days-of-tvision`

---

## DELETE List

### Agent infrastructure (TS-era)
- `.pi/` — Pi agent infrastructure
- `.agents/` — Agent skill definitions
- `.claude/` — Claude Code hooks
- `.codex/` — Codex CLI config
- `.codex-logs/` — Codex session logs
- `AGENTS.md` — Agent instructions for TS version
- `CLAUDE.md` — Claude Code instructions

### TypeScript/Bun artifacts
- `package-lock.json` — npm lockfile (no package.json on this branch)

- `spikes/rezi-tui-mvp/` — Bun/TS spike
- `spikes/ts-tui-mvp/` — Bun/TS spike (seed of the TS rewrite)
- `.sprite` — Sprites.dev config

### Session logs / CI
- `archive/` — Codex session logs from Feb 2026
- `logs/` — Agent mailbox logs, browser logs
- `.github/workflows/claude.yml` — Claude Code CI
- `.github/workflows/contract-tests.yml` — review if tests/ kept

### Planning (all TS-rewrite era)
- `.planning/` — All of it. Epics e005–e018 span transition but format is TS-era.

### Contracts
- `contracts/` — JSON schemas not referenced by C++ or Python API

### Docs (selective)
- `docs/migration/` — About migrating TO TypeScript
- `docs/codex-reviews/` — TS-era code reviews

### Tools (selective)
- `tools/agent_mailbox/` — TS-era experiment
- `tools/github/` — TS-era CI stuff

### Vendor/submodule cleanup
- `vendor/claude-system` — System prompt collection, tangential, not needed to build
- `modules-private` — Private submodule, won't resolve for anyone else

### Misc
- `.zilla/` — VS Code workspace file

---

## KEEP List

### Core C++ project
- `app/` — 193 files (85 .cpp, 82 .h) — the entire application
- `CMakeLists.txt` — Top-level CMake build
- `CMakePresets.json` — CMake presets
- `vendor/tvision` — Turbo Vision submodule
- `vendor/tvterm` — Terminal emulator widget submodule
- `vendor/MicropolisCore` — Micropolis engine submodule

### PartyKit (multichat bridge to C++)
- `partykit/` — PartyKit server, bridges to C++ room_chat_view

### Python API + tools
- `tools/api_server/` — FastAPI server with IPC to C++ app
- `tools/contour_stream.py` — Invoked by `contour_map_view.cpp`
- `tools/generative_stream.py` — Invoked by `generative_lab_view.cpp`
- `tools/generative_engine.py` — Imported by generative_stream
- `tools/arrange.py` — Window arrangement tooling
- `tools/smoke_parade.py` — Smoke test runner
- `tools/monitor/` — Instance monitor
- `tools/room/` — Room/multiplayer tools
- `tools/scripts/launch_tmux.sh` — Multi-instance tmux launcher (refs `build/app/wwdos`)
- `start_api_server.sh` — Root launch script

### Content assets (verified via C++ source grep)
- `modules/example-primers/` — 21 primer files, loaded by wwdos_app.cpp, wibwob_view.cpp
- `modules/wibwob-figlet-fonts/` — Figlet fonts, loaded by figlet_utils.h
- `paintings/` — .wwp paint files, loaded by paint_window.cpp
- `exports/` — Contour map exports, referenced by multiple views

### Scripts
- `scripts/dev-start.sh`, `scripts/dev-stop.sh` — C++ era launch
- `scripts/tmux-launch.sh` — C++ era tmux setup
- `scripts/init-submodules.sh` — Vendor setup
- `scripts/snap.sh`, `scripts/stamp.sh` — Utilities
- `scripts/parity-check.py`, `scripts/workspace_snapshot.py` — Small, keep
- `scripts/sprite-*.sh` — Sprite deployment scripts

### Tests
- `tests/` — Python tests for the C++ API server

### Docs (selective)
- `docs/architecture/` — C++ architecture docs
- `docs/manifestos/` — Historical interest
- `docs/master-philosophy.md` — Historical interest
- `docs/sprites/` — C++ sprite system docs
- `docs/readme-stub.md`, `docs/readme-improvement-prompt.md` — Small, keep
- `docs/development/` — Small, keep
- `docs/wibwobcity-gameplay.md` — Micropolis gameplay doc

### Historical / reference
- `README.md` — Rewrite for archive context
- `HANDOVER.md` — C++ era dev handover (Feb 23, scramble-visual-polish)
- `context.md` — C++ MCP tools layer audit
- `workings/` — Turbo Pascal research, scramble diagnosis, game PRDs
- `memories/` — C++ era session memories
- `screenshots/` — Historical screenshots of the C++ TUI
- `scratch/` — Contour map Python scripts
- `config/sprites/` — Sprite config
- `rooms/` — Multiplayer room examples
- `.env.example` — Environment config template
- `.gitignore` — Audit for C++ relevance

### GitHub
- `.github/pull_request_template.md` — Keep
- `.github/ISSUE_TEMPLATE/` — Keep (generic enough)

---

## Post-Delete Cleanup

### 1. `.gitmodules` — edit to remove stale entries
Remove:
```
[submodule "/Users/james/Repos/wibandwob-dos/vendor/tvision"]
    (absolute path — broken)
[submodule "vendor/claude-system"]
    (deleted)
[submodule "modules-private"]
    (deleted)
```
Keep:
```
[submodule "vendor/tvision"]
[submodule "vendor/MicropolisCore"]
[submodule "vendor/tvterm"]
```

### 2. `.gitignore` — audit
Remove TS-specific entries (node_modules, bun.lock, etc.).
Add C++ build entries if missing (build/, cmake-build-*/).

### 3. README.md — rewrite
Concise archive README:
- What: archived TVision/C++ version of WibWob-DOS
- Screenshot: `screenshots/wibwobdos-UI-collage.png`
- How to build: CMake + submodule init
- What the Python API server does
- Link to current TS version: `github.com/j-greig/wibandwob-dos`
- Historical context

### 4. Branch swap
```bash
git push origin last-days-of-tvision:main --force
gh repo edit j-greig/wibandwob-dos-tvision --default-branch main
git push origin --delete last-days-of-tvision
git checkout -B main origin/main
git branch -D last-days-of-tvision
```
Old `main` preserved in `j-greig/wibandwob-dos-backup`.

---

## Execution Order

1. Checkout `last-days-of-tvision`
2. Delete all items in DELETE list
3. Edit `.gitmodules` — remove stale entries
4. Audit `.gitignore`
5. Rewrite `README.md`
6. Commit: `chore: strip TS/agent cruft — standalone TVision C++ archive`
7. Verify: `cmake -B build && cmake --build build` (if submodules init'd)
8. Force-push `last-days-of-tvision` → `main`: `git push origin last-days-of-tvision:main --force`
9. Set default branch: `gh repo edit j-greig/wibandwob-dos-tvision --default-branch main`
10. Delete old remote branch: `git push origin --delete last-days-of-tvision`
11. Local cleanup: `git checkout -B main origin/main && git branch -D last-days-of-tvision`
