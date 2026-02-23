# Epic Status Register

> Machine-parseable index. One line per epic: `<dir> — <status>`.
> Valid statuses: `not-started`, `in-progress`, `blocked`, `done`, `dropped`.
> Canonical detail lives in each epic's frontmatter and GitHub issue.

<today>
## Completed — 2026-02-23

### E012 · ASCII Gallery & Wib Vision ✓ DONE (PR #82 merged)
- [x] 8 layout algorithms: masonry, fit_rows, masonry_horizontal, packery, cells_by_row, poetry, cluster, stamp
- [x] Pixel font stamp (3×5 dot matrix, text/grid/wave/diagonal/cross/border/spiral, dot_size param)
- [x] Window chrome: frameless × shadowless independent flags + show_title + force_open
- [x] MCP surface: operation_ids, /gallery/clear, full schema docs
- [x] gallery.py extracted from main.py, CLAUDE.md quick-start + chrome truth table
- [x] Friction log F1–F9, 29 stamp experiments documented

## What's Next — candidates

| Epic | Status | Effort | Value |
|---|---|---|---|
| E013 workspace save/restore parity | in-progress | medium | high — gallery arrangements should be saveable/restorable |
| E006 scramble TUI presence | in-progress | unknown — stalled | medium |
| E008 multiplayer PartyKit | in-progress | unknown — stalled | medium |
| E005 theme runtime wiring | not-started | small | low-medium |
| E011 desktop shell | not-started | large | high |

**Recommended next: E013** — already scoped, directly complements E012 (save a gallery arrangement, restore it). Round-trip test will also flush out remaining window registry gaps.

---
</today>

e001-command-parity-refactor — done
e002-browser-and-state-export — done
e003-dark-pastel-theme — done
e004-browser-rendering-reliability — dropped
e005-theme-runtime-wiring — not-started
e006-scramble-tui-presence — in-progress
e007-browser-hosted-deployment — done
e008-multiplayer-partykit — in-progress
e009-menu-system-redesign — done
e010-paint-canvas-integration — done
e011-desktop-shell — not-started
e012-ascii-gallery-and-wib-vision — done
e013-workspace-save-restore-parity — in-progress
