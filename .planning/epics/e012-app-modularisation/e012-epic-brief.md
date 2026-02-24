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
- [ ] S07: Binary rename — CMake target + launcher scripts (`test_pattern` → `wwdos`)
  Test: `cmake --build build` produces `build/wwdos`; `scripts/dev-start.sh` launches correctly
- [ ] S08: Socket path rename (`/tmp/test_pattern_app.sock` → `/tmp/wwdos.sock`) with legacy fallback
  Test: IPC connects on new path; old path still accepted for one transition cycle
- [ ] S09: Split workspace code (lines 274–369, 3301–4464) → `app/workspace/`
  Test: workspace save/load/manage works end-to-end; ctests pass
- [ ] S10: Route `api_ipc` paint handlers through registry — remove dual-dispatch
  Test: paint commands work via both IPC and registry; no duplicate handler code
- [ ] S11: Extract window chrome context menu → shared `ChromeContextMenu` helper
  Test: `TFrameAnimationWindow` + `TFigletTextWindow` use shared helper; visual parity

### F03 — Major Refactors (later phase)

- [ ] S12: Slim `TTestPatternApp` to orchestration-only
- [ ] S13: Unify IPC protocol (JSON req/resp, single dispatch path)
- [ ] S14: Registry-driven workspace serialisation (each window type registers save/restore)
- [ ] S15: Restructure `app/` into subdirs + CMake internal libraries

### E013 — Rename (post-E012, separate epic)

Phases 3–5 of the rename spike are deferred. See spike for full blast-radius analysis.
Spike: `.planning/spikes/spk-wwdos-rename/spk01-test-pattern-to-wwdos.md`

- [ ] Class rename `TTestPatternApp` → `TWwdosApp` (445 hits across 15 files)
- [ ] Source file rename `test_pattern_app.cpp` → `wwdos_app.cpp` (30 refs / 18 files)
- [ ] Docs/skills/planning sweep (24 files)

Trigger: open E013 issue immediately after E012 merges to main.

## Acceptance Criteria

| AC | Criterion | Test |
|----|-----------|------|
| AC-1 | No duplicate extern in `command_registry.cpp` | `grep -c 'api_open_animation_path' app/command_registry.cpp` == 1 |
| AC-2 | `inject_command` in capabilities | API response includes it |
| AC-3 | Single `json_escape()` function | One definition, zero alternates |
| AC-4 | `makeStringCollection()` helper used everywhere | Three old sites removed |
| AC-5 | `TFrameAnimationWindow` in `app/windows/` | Compiles + ctests pass |
| AC-6 | `.wwp` codec in `app/paint/paint_wwp_codec.*` | Round-trip test passes |
| AC-7 | Binary renamed to `wwdos` | `build/wwdos` exists; launchers updated |
| AC-8 | Socket path updated to `/tmp/wwdos.sock` | IPC connects; legacy fallback present |
| AC-9 | Workspace code in `app/workspace/` | Save/load/manage works |
| AC-10 | Dual-dispatch eliminated from `api_ipc` | No paint handler duplication |

## Risks

| Risk | Mitigation |
|------|-----------|
| 70 friend declarations make extraction hard | Extract incrementally; keep friends until class is slim enough to remove them |
| CMakeLists.txt breakage mid-refactor | Each extraction commits in isolation with passing build |
| Workspace codec regression | Keep old code until new codec is verified by round-trip test |
| S07/S08 rename breaks launchers/IPC | Legacy socket fallback required; verify all launcher scripts in same commit |

## Sequencing

Phase 1 (Quick Wins — F01): S01→S02→S03→S04 — independent, each a single commit
Phase 2 (Medium — F02): S05→S06→S07→S08→S09→S10→S11 — one story per PR slice
  S07/S08 are rename spike Phases 1–2 (binary + socket); safe here as CMakeLists.txt and
  api_ipc.cpp are already touched by S05/S06. See spike: `.planning/spikes/spk-wwdos-rename/spk01-test-pattern-to-wwdos.md`
Phase 3 (Major — F03): S12→S13→S14→S15 (renumbered) — after Phase 2 is done

Note: rename spike Phases 3–5 (class rename TTestPatternApp→TWwdosApp, file rename, docs sweep)
are deferred to E013 — blast radius (445 hits) is smaller after E012 slims the god-object.

## Rollback

All Phase 1/2 extractions are additive moves — original logic unchanged, just relocated.
Revert any story commit to restore prior state.
