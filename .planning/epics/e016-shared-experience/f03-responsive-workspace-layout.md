---
title: "F03 — Responsive Workspace Layout"
status: not-started
epic: E016
GitHub issue: —
PR: —
---

# F03 — Responsive Workspace Layout

## TL;DR
Workspace JSON positions are absolute pixel/cell coordinates, so layouts designed for one screen size break on smaller/larger terminals. Need a positioning system that adapts to actual desktop dimensions at restore time.

## Problem
- Sprites users connect via browser at wildly different terminal sizes (80×24 to 300×80+)
- Current workspace restore clamps windows to fit but doesn't reposition them
- Primers designed for 160-col screen overlap room chat on 100-col terminal
- No way to express "right edge", "centre", "percentage width" in workspace JSON

## Current State (what we have)
- `"anchor": "right"` / `"bottom"` support added to workspace restore (E015) — x/y measured from opposite edge
- Clamping: windows that overflow desktop bounds get pushed inward
- `config/sprites/default-layout.json` — Sprites-specific workspace loaded via `WIBWOB_LAYOUT_PATH`
- Left-anchored layout sidesteps the problem for now (primers left, chat right)

## Hard Parts
1. **Scaling vs anchoring**: Percentage-based widths need a "design resolution" reference in the JSON. Current `screen` field exists but isn't used for scaling — only for metadata
2. **Overlapping windows**: If two windows both want "right half", they stack. Need z-order or stacking rules
3. **Font/emoji width**: Primer art has fixed character widths (e.g. chaos-vs-order = 100 cols). Can't shrink these — they'd break. So responsive = repositioning, not resizing
4. **Frameless windows**: No title bar to grab, so if positioned off-screen the user can't recover them
5. **Multiple layout files**: Could ship `sprites-small.json`, `sprites-wide.json` etc, but then session script needs to detect terminal size and pick one

## Proposed Approach (TBD)
Options in rough order of complexity:

### A. Anchor + clamp (current, ~80% there)
- `"anchor": "right"` already works for right-edge positioning
- Add `"anchor": "center"` for horizontal centering
- Clamp already prevents off-screen — just needs testing at various sizes
- **Limitation**: Fixed-width primers can't adapt to narrow terminals

### B. Breakpoint layouts
- Multiple JSON files: `default-layout-80.json`, `default-layout-120.json`, `default-layout-160.json`
- Session script detects `$COLUMNS` and picks closest match
- Simple, predictable, easy for humans to author
- **Limitation**: More files to maintain

### C. Layout expressions
- Replace fixed `x`/`y` with expressions: `"x": "100% - 80"`, `"x": "50%"`
- Needs an expression parser in C++ (or a simple substitution engine)
- Most flexible but most complex
- **Risk**: Over-engineering for a TUI app

## Acceptance Criteria
- AC-1: Primers visible and non-overlapping on 80×24 terminal
  - Test: Load workspace, check no window extends beyond desktop bounds
- AC-2: Room chat usable width on 100-col terminal (min 50 cols)
  - Test: Load workspace at 100 cols, verify chat window ≥ 50 cols wide
- AC-3: Layout looks intentional (not just "clamped") on 120+ col terminal
  - Test: Visual check — windows have breathing room, not all jammed left

## Open Questions
- Is Option A (anchor + clamp) good enough for V1?
- Should narrow terminals just hide primers and show only room chat?
- Do we need runtime re-layout on terminal resize, or only at startup?
