---
title: "Spike — Desktop Texture, Colour & Gallery Mode"
status: not-started
created: 2026-02-23
related_epic: e005-theme-runtime-wiring
github_issue: "—"
PR: "—"
---

# Desktop Texture, Colour & Gallery Mode

Three things: pick the background fill character, pick its colours, and toggle "gallery mode" that hides the menu bar + status line for a clean wall.

## Background

`TBackground` fills the desktop with one repeated char in one colour. Currently `▒` in blue/grey. To change at runtime: mutate pattern + colour, call `drawView()`. Done.

Gallery mode hides `TCustomMenuBar` (row 0) and `TCustomStatusLine` (bottom row), then grows the desktop to fill the freed space. Result: a clean canvas — art gallery wall, no chrome.

## What to Build

### 1. `TWibWobBackground` (C++ subclass)

```cpp
class TWibWobBackground : public TBackground {
    uchar fgColor, bgColor;
public:
    void setTexture(char ch);
    void setColor(uchar fg, uchar bg);
    void draw() override; // explicit TColorAttr, bypasses palette
};
```

Wire via `initDeskTop()` or replace-after-init.

### 2. Gallery Mode (hide chrome)

```cpp
void TTestPatternApp::setGalleryMode(bool on) {
    menuBar->setState(sfVisible, !on);
    statusLine->setState(sfVisible, !on);
    // Recalc desktop bounds to fill freed rows
    TRect r = getExtent();
    r.a.y = on ? 0 : 1;
    r.b.y = on ? r.b.y : r.b.y - 1;
    deskTop->changeBounds(r);
}
```

Toggle: keyboard shortcut (e.g. `Alt+G` or `F11`) + IPC command + API endpoint.  
Re-show on any menu-access key (`F10`, `Alt+letter`) as safety escape.

### 3. IPC / API / MCP

| Endpoint | Body | Effect |
|----------|------|--------|
| `POST /desktop/preset` | `{"preset": "jet_black"}` | Apply named preset |
| `POST /desktop/texture` | `{"char": "░"}` | Set fill character |
| `POST /desktop/color` | `{"fg": 7, "bg": 1}` | Set fg/bg (CGA 0–15) |
| `POST /desktop/gallery` | `{"on": true}` | Toggle gallery mode |
| `GET /desktop` | — | Current state |

IPC commands: `cmDesktopTexture`, `cmDesktopColor`, `cmDesktopPreset`, `cmDesktopGallery`.

### 4. Menu UI

Under `View` menu:

```
Desktop ▶  Texture ▶  ░ Light | ▒ Medium | ▓ Dark | █ Solid | (space) Empty
           Colour  ▶  Black | Blue | Cyan | Green | ...
           Gallery Mode  (Alt+G)
```

### 5. Workspace Persistence

Add to `buildWorkspaceJson()` / `restoreWorkspace()`:

```json
{ "desktop": { "char": "▒", "fg": 7, "bg": 1, "gallery": false, "preset": "default" } }
```

## Presets

| Name | Char | BG | FG | Notes |
|------|------|----|----|-------|
| `default` | ▒ | Blue (1) | Light Grey (7) | Classic TV |
| `jet_black` | ` ` | Black (0) | Black (0) | Void |
| `terminal` | ░ | Black (0) | Dark Grey (8) | CRT |
| `cga_cyan` | ▒ | Cyan (3) | White (15) | Retro bright |
| `cga_green` | ░ | Black (0) | Bright Green (10) | Phosphor |
| `noise` | % | Black (0) | Dark Grey (8) | Grungy |
| `white_paper` | ` ` | White (15) | White (15) | Blank canvas |
| `gallery_wall` | ` ` | Black (0) | Black (0) | Gallery mode default — void + no chrome |

## Acceptance Criteria

- **AC-1:** `POST /desktop/preset {"preset": "jet_black"}` changes desktop immediately.
  - Test: API call → screenshot comparison (bg changed).
- **AC-2:** `GET /desktop` returns current char, fg, bg, gallery, preset.
  - Test: GET after SET, assert JSON fields match.
- **AC-3:** Gallery mode hides menu bar + status line, desktop fills full terminal height.
  - Test: toggle on → `menuBar->getState(sfVisible) == false`, desktop bounds == full extent.
- **AC-4:** Gallery mode re-shows chrome on `F10` / `Alt+letter` / `Escape` as safety escape.
  - Test: gallery on → send F10 → menu bar visible again.
- **AC-5:** At least 8 named presets available.
  - Test: iterate preset list, apply each, verify no crash.
- **AC-6:** Desktop state persists across workspace save/restore.
  - Test: set preset → save workspace → restart → restore → GET matches.
- **AC-7:** Parity check script passes after adding new workspace props.
  - Test: run parity script, zero failures.

## Effort

| Phase | Est |
|-------|-----|
| TWibWobBackground + app setter | 2h |
| Gallery mode (hide/show/resize) | 1h |
| API + IPC + MCP | 1h |
| Menu UI | 30m |
| Workspace persistence | 30m |
| **Total** | **~5h** |

## Open Questions

- **Safety escape in gallery mode**: `F10` is natural (TV menu key). But should `Escape` also restore? Could conflict with dialog dismissal.
- **TrueColor**: tvision supports `TColorRGB` — expose hex input? Out of scope here, note for E005.
- **Wib agency**: Should Scramble be able to call `set_desktop` unprompted? Note for E006.
