---
Status: not-started
GitHub issue: —
PR: —
Type: spike
---

# SPK02: Window Z-Order Commands

## TL;DR

We have no way to control which window is in front or behind. `focus_window` only changes keyboard focus, it does NOT raise the window. The `/state` API reports `z: 0` for everything. This makes compositional work (figlet videographer, workspace layouts, art layering) impossible to control programmatically.

## Problem

1. `focus_window` calls `deskTop->setCurrent()` which sets keyboard focus but does not reorder z (TV's `select()` does both — we're using the wrong primitive)
2. `/state` hardcodes `z: 0` for all windows — agents and API consumers have no idea what's in front
3. No commands exist for: raise, lower, send-to-back, bring-to-front, or explicit z-ordering
4. The figlet videographer creates primer art windows that get buried behind text windows with no way to fix it

## Turbo Vision Internals (from analysis)

- Z-order IS the `TGroup` child linked list order. `first()` = frontmost, `last` = backmost (above background)
- `TView::putInFrontOf(target)` relinks a view in front of target — this is THE primitive for z manipulation
- `TWindow::select()` calls `makeFirst()` which calls `putInFrontOf(owner->first())` — this raises AND focuses
- `cmPrev` / send-to-back uses `putInFrontOf(background)` — sends to back of stack
- `prev()` is O(n) so bulk reordering many windows is O(n^2) — fine for typical window counts (<50)

### Gotchas

- `putInFrontOf()` on visible selectable views calls `owner->resetCurrent()` — focus may change unexpectedly
- Background is a view in the same list — z APIs must exclude it when indexing
- `putInFrontOf()` constraints: same owner, not self, not nextView
- No native integer z-index — we'd assign indices from list traversal order

## Proposed Commands

### Minimal (do this first)

| Command | Args | Implementation | Notes |
|---------|------|----------------|-------|
| `raise_window` | `id` | `w->select()` | Bring to front + focus |
| `lower_window` | `id` | `w->putInFrontOf(deskTop->background)` | Send to back, keep focusable |

### Fix existing

| Change | Detail |
|--------|--------|
| Fix `focus_window` | Either rename to `raise_window` or change impl to `w->select()` — current name is misleading |
| Fix `/state` z field | Emit real z-index from C++ traversal order. `0` = frontmost, incrementing toward back |
| Fix `/state` focused field | Emit `focused: true` for `deskTop->current` |

### Extended (if needed)

| Command | Args | Implementation | Notes |
|---------|------|----------------|-------|
| `set_z_order` | `ids` (ordered list) | Walk list, `putInFrontOf` chain | `ids[0]` = front. Omitted windows keep relative order |
| `swap_z` | `id1`, `id2` | Swap positions in list | For precise two-window reorder |
| `raise_above` | `id`, `target` | `w->putInFrontOf(target)` | Place id directly in front of target |

## Implementation Scope

### C++ (app layer)

- `app/command_registry.cpp`: Add `raise_window`, `lower_window` command entries + dispatch
- `app/wwdos_app.cpp`: Implement helper functions, fix `api_get_state()` to emit `z` and `focused_id`
- `app/api_ipc.cpp`: Wire IPC dispatch if separate from command registry path

### Python (API layer)

- `tools/api_server/controller.py`: Update `_sync_state()` to use real `z` and `focused` from C++ response instead of hardcoded `0`/`False`
- No new endpoints needed — everything goes through `POST /menu/command`

### Tests

- IPC chain test: create 3 windows, raise middle one, verify order in `/state`
- Lower test: lower front window, verify it's now last
- Focus semantics: verify `focus_window` behaviour matches its documentation (fix whichever is wrong)
- Edge cases: invalid ID, single window, already-front, already-back
- State parity: `/state` z values match actual render order after reordering

## Risks

- `resetCurrent()` side-effect: raising a window may steal focus from a chat input or terminal — need to test interactive windows
- Agents may over-use z-ordering and create flickery layouts — document best practice (place windows in desired order rather than reordering after)
- `set_z_order` with many windows could be slow — probably fine under 50 windows

## Acceptance Criteria

- AC-1: `raise_window` brings a window to front visually (verified by screenshot or `/state` z=0)
  - Test: create 3 windows, raise the last, check `/state` order
- AC-2: `lower_window` sends a window to back visually
  - Test: create 3 windows, lower the first, check `/state` order
- AC-3: `/state` reports accurate z-index for all windows (0=front, incrementing)
  - Test: compare `/state` z values against tmux capture render order
- AC-4: `/state` reports which window is focused
  - Test: focus different windows, verify `focused` field changes
- AC-5: `focus_window` semantics are documented and match implementation
  - Test: call focus_window, verify behaviour matches docs

## Motivation

The figlet videographer creates layered compositions where ASCII art primers should sit behind or in front of text. Without z-control, art windows with oversized frames (73w for 28-char art) obscure the text beneath. This spike unblocks compositional control for all window-based creative tools.

## Open Questions

- Should `raise_window` also focus, or should raise and focus be separate operations?
- Should `focus_window` be deprecated/renamed to avoid confusion?
- Do we want `raise_window` without focus (just visual reorder, no keyboard steal)? TV makes this tricky due to `resetCurrent()`.
