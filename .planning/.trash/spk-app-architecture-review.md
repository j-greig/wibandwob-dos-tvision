# Spike: App Architecture Review & Refactor Plan

Status: done
GitHub issue: —
PR: —
Branch: main

## Summary

Deep architectural review of the WibWob-DOS codebase by Codex analyst.
The main finding: `test_pattern_app.cpp` is a 5382-line god-object with
70 friend declarations, duplicated logic, and mixed responsibilities.

## Key Findings

### 1. God-Object: test_pattern_app.cpp (5382 lines)

| Lines | Section | Extract To |
|-------|---------|-----------|
| 1-150 | Includes, primer paths | `app/core/primer_paths.cpp` |
| 151-273 | Command ID constants | `app/ui/app_commands.h` |
| 274-369 | Recent workspace scanning | `app/workspace/recent_workspaces.cpp` |
| 381-496 | TCustomMenuBar | `app/ui/custom_menu_bar.cpp` |
| 497-592 | TCustomStatusLine | `app/ui/custom_status_line.cpp` |
| 594-660 | TTestPatternView/Window | `app/views/test_pattern_window.cpp` |
| 661-725 | TGradientWindow | `app/windows/gradient_window.cpp` |
| 726-869 | TFrameAnimationWindow | `app/windows/frame_animation_window.cpp` |
| 872-1124 | TTestPatternApp class + 70 friends | `app/core/test_pattern_app.h` |
| 1178-1874 | handleEvent mega-switch | Split by domain |
| 2911-3299 | API helpers (spawn/window ops) | `app/api/api_windows.cpp` |
| 3301-3475 | Manual JSON parse helpers | `app/workspace/json_miniparser.cpp` |
| 3476-4464 | Workspace load/save/manage | `app/workspace/workspace_manager.cpp` |
| 4464-5382 | More API helpers (paint/figlet/etc) | `app/api/api_paint.cpp` etc |

### 2. command_registry.cpp

- 52 registered commands, 53 dispatch branches
- 54 `extern api_*` declarations — flat, not grouped
- Duplicate extern for `api_open_animation_path`
- Hidden `inject_command` not in capabilities list

### 3. api_ipc.cpp — Dual Dispatch Problem

Two separate command dispatch paths:
- Direct IPC commands in `ApiIpcServer::poll()` (lines 397-758)
- Registry-based commands via `exec_command`

This creates duplication — paint commands are handled in BOTH paths.
**Recommendation**: Make `api_ipc` transport-only, route all commands through registry.

### 4. Biggest DRY Violations

**Triple-maintained .wwp codec** (worst offender):
- `paint_window.cpp`: buildWwpJson + parseIntAfter/parseBoolAfter + doOpen
- `test_pattern_app.cpp api_paint_save`: re-implements JSON serialization
- `test_pattern_app.cpp api_paint_load`: re-implements parsing
- `test_pattern_app.cpp api_spawn_paint_with_file`: third copy of parse loop

**Duplicated JSON escape**:
- `command_registry.cpp:134` json_escape
- `test_pattern_app.cpp:3808` jsonEscape  
- Ad-hoc escape loops in api_ipc.cpp

**Duplicated window chrome context menus**:
- TFrameAnimationWindow (in test_pattern_app.cpp)
- TFigletTextWindow (in figlet_text_view.cpp)
- Same shadow/title/frame/gallery toggles

**Duplicated TStringCollection population**:
- figlet_text_view.cpp font list
- figlet_stamp_dialog.cpp font list  
- test_pattern_app.cpp workspace manager list

**Window sizing** (already in spk-window-sizing-helpers):
- 6+ sites doing the same centering math

### 5. Proposed Module Structure

```
app/core/       — thin app shell, command IDs, window registry
app/ui/         — menu bar, status line, file dialog helpers
app/windows/    — frame_animation, gradient, chrome context, sizing
app/workspace/  — codec, manager, preview, manage dialog, recent
app/api/        — ipc transport, command registry, domain handlers
app/figlet/     — utils, text view, shared font dialog
app/paint/      — (already exists, well-organized)
app/desktop/    — wibwob_background
app/views/      — all view types (mostly move existing files)
```

### 6. Missing Abstractions

- `ChromableContentWindow` base — unify frame/shadow/title toggles
- `PaintWwpCodec` — shared .wwp parse/emit used by UI and API
- `WorkspaceCodec` + `WindowStateCodec` — registry-driven serialization
- `CommandSpec` table — replace long if/else dispatch chain
- `makeStringCollection()` helper — one-liner for vector→TStringCollection

### 7. Extensibility: Adding a New Window Type

Currently requires 5-9 touchpoints:
1. View/window .cpp/.h + CMakeLists
2. Menu command constant + menu item + handleEvent case
3. Optional API spawn + friend declaration (70 friends!)
4. window_type_registry spec entry
5. Workspace save/load special-case
6. command_registry capability + dispatch

**Fix**: Expand `window_type_registry` to include persistence and menu metadata.

### 8. Priority Ordering

#### Quick Wins (< 1 hour each)
- [x] Window sizing helpers (spike exists)
- [ ] Remove duplicate extern in command_registry.cpp
- [ ] Register inject_command in capabilities
- [ ] Centralize json_escape helper
- [ ] makeStringCollection() helper

#### Medium Refactors (1-4 hours each)
- [ ] Extract TFrameAnimationWindow to own file
- [ ] Extract .wwp codec to shared paint_wwp_codec.*
- [ ] Split workspace code from test_pattern_app.cpp
- [ ] Route api_ipc paint handlers through registry (remove duplication)
- [ ] Extract window chrome context menu helper

#### Major Refactors (days)
- [ ] Slim TTestPatternApp to orchestration-only
- [ ] Unify IPC protocol (JSON requests/responses, single dispatch)
- [ ] Registry-driven workspace serialization for all window types
- [ ] Restructure app/ into modules + CMake internal libraries

#### Stretch Goals

> Note: additional DRY/extensibility opportunities found while working. Add more here as they surface.

- [ ] `file_dialog_helpers.*` — 7 nearly-identical `new TFileDialog(...)` blocks across `test_pattern_app.cpp` (L1843, L1861, L2344, L2396) and `paint_window.cpp` (L322, L381, L479) → one helper
- [ ] Shared atomic-write helper — `saveWwpFile()` + `saveWorkspacePath()` both do temp-write-then-rename → one `atomicWriteFile(path, content)` utility
- [ ] `IWindowStateCodec` interface — formal interface for window types to implement save/restore props (contract for future registry-driven workspace serialisation)
- [ ] `CommandSpec` table-driven dispatch — replace 53-branch `if (name == ...)` chain in `command_registry.cpp:168` with a static dispatch table
- [ ] IPC / capabilities parity audit — 16 IPC-direct commands not in registry caps; 39 registry caps not in IPC direct; document intent for each
- [ ] `SpawnAndCaptureWindow` helper — before/after desktop scan pattern repeated in workspace load for registry spawns
- [ ] CMake internal libraries — `app_ui`, `app_workspace`, `app_api`, `app_windows` as `add_library(STATIC)` targets once modules exist

## Source

Full Codex analyst report: `.codex-logs/2026-02-24/codex-do-a-comprehensive-architectur-2026-02-24T09-36-22.log`
Note: there be other opps to make the codebase more DRY and modular and future-extensible, add these to a Stretch Goals list above if you come across additional stuff as you work on this.