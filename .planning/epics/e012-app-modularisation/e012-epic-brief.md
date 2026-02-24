---
id: E012
title: App Modularisation — God-Object Refactor, DRY Fixes & Module Structure
status: in-progress
issue: 90
pr: ~
depends_on: []
branch: epic/e012-app-modularisation
---

# E012 — App Modularisation

## TL;DR

`test_pattern_app.cpp` is 5382 lines and does everything. Extract it into clean modules.
Quick wins land first (< 1 hr each), then medium refactors, then the major structural work.

## Objective

Split `test_pattern_app.cpp` (5382-line god-object, 70 friend declarations) into focused
modules per the architecture spike. Eliminate the three worst DRY violations. Make adding
a new window type require ≤ 3 touchpoints instead of 5–9.

## Source Spike

`.planning/spikes/spk-app-architecture-review.md`
Codex analyst log: `.codex-logs/2026-02-24/codex-do-a-comprehensive-architectur-2026-02-24T09-36-22.log`

## Proposed Module Structure

```
app/core/       — thin app shell, command IDs, window registry
app/ui/         — menu bar, status line, file dialog helpers
app/windows/    — frame_animation, gradient, chrome context, sizing
app/workspace/  — codec, manager, preview, manage dialog, recent
app/api/        — ipc transport, command registry, domain handlers
app/figlet/     — utils, text view, shared font dialog
app/paint/      — (already exists, well-organised)
app/desktop/    — wibwob_background
app/views/      — all view types
```

## Features / Stories

### F01 — Quick Wins

- [ ] S01: Remove duplicate `api_open_animation_path` extern in `command_registry.cpp`
  Test: `grep -c 'api_open_animation_path' app/command_registry.cpp` == 1
- [ ] S02: Register `inject_command` in capabilities list
  Test: `GET /api/capabilities` includes `inject_command`
- [ ] S03: Centralise `json_escape` — single helper used by all three call sites
  Test: one definition, no `jsonEscape` or inline escape loops remaining
- [ ] S04: `makeStringCollection()` helper replaces three duplicate vector→TStringCollection sites
  Test: helper exists; `figlet_text_view`, `figlet_stamp_dialog`, workspace manager all use it

### F02 — Medium Refactors

- [ ] S05: Extract `TFrameAnimationWindow` → `app/windows/frame_animation_window.cpp/h`
  Test: compiles, all ctests pass
- [ ] S06: Extract `.wwp` codec → `app/paint/paint_wwp_codec.cpp/h` (shared by UI + API)
  Test: paint save/load round-trip passes; `buildWwpJson` / `parseWwp` no longer duplicated
- [ ] S07: Split workspace code (lines 274–369, 3301–4464) → `app/workspace/`
  Test: workspace save/load/manage works end-to-end; ctests pass
- [ ] S08: Route `api_ipc` paint handlers through registry — remove dual-dispatch
  Test: paint commands work via both IPC and registry; no duplicate handler code
- [ ] S09: Extract window chrome context menu → shared `ChromeContextMenu` helper
  Test: `TFrameAnimationWindow` + `TFigletTextWindow` use shared helper; visual parity

### F03 — Major Refactors (later phase)

- [ ] S10: Slim `TTestPatternApp` to orchestration-only
- [ ] S11: Unify IPC protocol (JSON req/resp, single dispatch path)
- [ ] S12: Registry-driven workspace serialisation (each window type registers save/restore)
- [ ] S13: Restructure `app/` into subdirs + CMake internal libraries

## Acceptance Criteria

| AC | Criterion | Test |
|----|-----------|------|
| AC-1 | No duplicate extern in `command_registry.cpp` | `grep -c 'api_open_animation_path' app/command_registry.cpp` == 1 |
| AC-2 | `inject_command` in capabilities | API response includes it |
| AC-3 | Single `json_escape()` function | One definition, zero alternates |
| AC-4 | `makeStringCollection()` helper used everywhere | Three old sites removed |
| AC-5 | `TFrameAnimationWindow` in `app/windows/` | Compiles + ctests pass |
| AC-6 | `.wwp` codec in `app/paint/paint_wwp_codec.*` | Round-trip test passes |
| AC-7 | Workspace code in `app/workspace/` | Save/load/manage works |
| AC-8 | Dual-dispatch eliminated from `api_ipc` | No paint handler duplication |

## Stretch Goals

> Note: additional DRY/extensibility opportunities found while working. Add more here as they surface.

- [ ] `file_dialog_helpers.*` — 7 nearly-identical `new TFileDialog(...)` + cancel-flow blocks across `test_pattern_app.cpp` (L1843, L1861, L2344, L2396) and `paint_window.cpp` (L322, L381, L479) → one `openFileDialog(glob, title, flags)` helper
- [ ] Shared atomic-write helper — `saveWwpFile()` in `paint_window.cpp` and `saveWorkspacePath()` in workspace manager use the same temp-write-then-rename dance → one `atomicWriteFile(path, content)` utility
- [ ] `IWindowStateCodec` interface — formal interface for window types to implement save/restore props (feeds into S12 registry-driven workspace serialisation; defines the contract early)
- [ ] `CommandSpec` table-driven dispatch — replace 53-branch `if (name == ...)` chain in `command_registry.cpp:168` with a static dispatch table keyed by name
- [ ] IPC / capabilities parity audit — 16 IPC-direct commands not in registry caps; 39 registry caps not in IPC direct; document which direction each should resolve (some intentionally IPC-only, some should be promoted to registry)
- [ ] `SpawnAndCaptureWindow` helper — before/after desktop scan pattern repeated multiple times in workspace load for registry spawns → one helper
- [ ] CMake internal libraries — once modules are split, define `app_ui`, `app_workspace`, `app_api`, `app_windows` as `add_library(STATIC)` targets to shrink the monolithic source list in `CMakeLists.txt`

## Risks

| Risk | Mitigation |
|------|-----------|
| 70 friend declarations make extraction hard | Extract incrementally; keep friends until class is slim enough to remove them |
| CMakeLists.txt breakage mid-refactor | Each extraction commits in isolation with passing build |
| Workspace codec regression | Keep old code until new codec is verified by round-trip test |

## Sequencing

Phase 1 (Quick Wins — F01): S01→S02→S03→S04 — independent, each a single commit
Phase 2 (Medium — F02): S05→S06→S07→S08→S09 — one story per PR slice
Phase 3 (Major — F03): S10→S11→S12→S13 — after Phase 2 is done

## Rollback

All Phase 1/2 extractions are additive moves — original logic unchanged, just relocated.
Revert any story commit to restore prior state.
