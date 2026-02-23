# E013 — Workspace Save/Restore Parity

Status: not-started
GitHub issue: —
PR: —

## Problem

Workspace save/restore has lost parity with the live TUI. Windows are serialised with wrong types, missing props, and missing titles. Restoring a saved workspace produces blank or wrong windows.

Evidence from `workspaces/last_workspace_260223_0849.json`:
- ASCII Gallery saved as `type: "test_pattern"` (wrong slug)
- All 14 frame_player windows have `props: {}` — no `path` or `fps` persisted
- All frame_player windows have `title: ""` — filenames lost
- Gallery-specific state (selected tab, focused file) not captured

## Root Causes

1. **Window type registry gaps** — `ascii_gallery` not registered as a serialisable type; serialiser falls back to `test_pattern`.
2. **Missing `getProps()` overrides** — `TFrameFilePlayerView`, `TTextFileView`, `TAsciiGalleryWindow`, and likely others don't return their file path, fps, or other state from `getProps()`.
3. **Missing `setProps()` restore path** — even if props were saved, the restore path doesn't know how to reconstruct windows with those props.
4. **No round-trip test** — no automated test that save→restore→save produces equivalent JSON.

## Scope

### Feature F01 — Window Type Registry Completeness
- [ ] AC-01: Every window type that can be created (menu or API) has a registered type slug in `window_type_registry`
  - Test: registry slug count ≥ number of unique `api_spawn_*` functions
- [ ] AC-02: `ascii_gallery` type registered and serialises correctly
  - Test: save workspace with gallery open → JSON has `type: "ascii_gallery"`

### Feature F02 — Props Serialisation
- [ ] AC-03: `TFrameFilePlayerView::getProps()` returns `{path, fps}`
  - Test: save workspace with frame_player → JSON props contain `path` and `fps`
- [ ] AC-04: `TTextFileView::getProps()` returns `{path}`
  - Test: save workspace with text_view → JSON props contain `path`
- [ ] AC-05: `TGalleryWindow::getProps()` returns `{tab, search, focused}`
  - Test: save workspace with gallery → JSON props contain gallery state
- [ ] AC-06: Audit all window types for missing `getProps()` — fix each
  - Test: contract test comparing live window props vs serialised props

### Feature F03 — Props Restore
- [ ] AC-07: `frame_player` restore creates window with correct file and fps
  - Test: restore workspace JSON with frame_player path → window shows correct animation
- [ ] AC-08: `text_view` restore opens correct file
  - Test: restore workspace JSON with text_view path → window shows correct content
- [ ] AC-09: `ascii_gallery` restore opens with correct tab/search/focus
  - Test: restore gallery workspace → correct tab selected

### Feature F04 — Round-Trip Test
- [ ] AC-10: Automated test: create windows → save → restore → save → compare JSON
  - Test: `ctest` round-trip test produces equivalent workspace JSON (modulo IDs/timestamps)

### Feature F05 — Window Title Persistence
- [ ] AC-11: Window titles serialised from `TWindow::getTitle()` not empty string
  - Test: frame_player with primer file → saved title matches filename
- [ ] AC-12: Restored windows have correct titles
  - Test: restore → all window titles match saved JSON

## Cross-References

- Window spawn audit: `.planning/audits/window-spawn-audit-20260223.md`
- Window type registry: `app/window_type_registry.cpp`
- Workspace save: `app/test_pattern_app.cpp` (search `saveWorkspace`)
- Workspace restore: `app/test_pattern_app.cpp` (search `openWorkspace`)
- Related: E011 (desktop shell), spk-auth-unification (window spawn DRY refactor)

## Effort Estimate

Medium-Large. Touches every view class, the type registry, save/restore paths, and needs new contract tests. Recommend slicing F01+F02 first (serialisation), then F03 (restore), then F04 (round-trip test).
