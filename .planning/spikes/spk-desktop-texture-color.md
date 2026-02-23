---
title: "Spike — Desktop Texture & Colour Options"
status: not-started
created: 2026-02-23
related_epic: e005-theme-runtime-wiring
github_issue: "—"
PR: "—"
---

# TL;DR

WibWobDOS desktop is currently a single solid fill character (▒ or similar) in one fixed colour. This spike scopes what it would take to let the user (or Wib&Wob) cycle or set the desktop **texture** (fill character) and **colour** (fg/bg) at runtime — including a CGA palette and the full ASCII shade-block spectrum.

---

## Background

The desktop background in Turbo Vision is a `TBackground` view — a dead-simple `TView` subclass with two properties:

```cpp
char pattern;          // fill character, e.g. ' ', '░', '▒', '▓', '█'
getColor(0x01);        // one palette byte = fg nibble | bg nibble << 4
```

`draw()` just floods the canvas with `pattern` in whatever colour palette entry 0x01 resolves to. There is no animation, no gradient, no tiling — just one char repeated.

To change texture or colour at runtime you only need to:
1. Mutate `background->pattern` (or `fg`/`bg` colour fields in a subclass), then
2. Call `background->drawView()` (forces a repaint).

The colour resolution path: `getColor(0x01)` → app palette → `TColorAttr` (supports both classic 4-bit and TrueColor RGB in modern tvision). Classic palette byte is `(bg << 4) | fg`, both 0–15.

---

## Option Set

### A. Texture — Fill Characters

| Label | Char | Description |
|---|---|---|
| `empty` | ` ` (space) | Solid background colour, no texture |
| `shade1` | `░` (U+2591) | Light shade block |
| `shade2` | `▒` (U+2592) | Medium shade block — **current default** |
| `shade3` | `▓` (U+2593) | Dark shade block |
| `solid` | `█` (U+2588) | Full block — no texture, looks like solid colour |
| `dot` | `·` | Subtle dot grid |
| `cross` | `+` | Grid/crosshatch feel |
| `wave` | `~` | Wavy texture |
| `noise` | `%` | Noisy/grungy feel |

These are a good starting MVP — 9 named presets. Could later extend to arbitrary char input.

### B. Colour — CGA Palette (Classic DOS)

The original IBM CGA palette — 16 colours, still the soul of any DOS aesthetic:

| Index | Name | HTML approx |
|---|---|---|
| 0 | Black | #000000 |
| 1 | Blue | #0000AA |
| 2 | Green | #00AA00 |
| 3 | Cyan | #00AAAA |
| 4 | Red | #AA0000 |
| 5 | Magenta | #AA00AA |
| 6 | Brown | #AA5500 |
| 7 | Light Grey | #AAAAAA |
| 8 | Dark Grey | #555555 |
| 9 | Bright Blue | #5555FF |
| 10 | Bright Green | #55FF55 |
| 11 | Bright Cyan | #55FFFF |
| 12 | Bright Red | #FF5555 |
| 13 | Bright Magenta | #FF55FF |
| 14 | Yellow | #FFFF55 |
| 15 | White | #FFFFFF |

For desktop background (`TBackground`), **bg colour** is the dominant choice. Fg matters only for shade chars (░▒▓) where the fg+bg pair creates the visual density.

MVP scope: **pick bg colour from CGA 16**. Stretch: pick fg independently. Further stretch: TrueColor RGB via `TColorRGB`.

---

## Implementation Plan

### Phase 1 — Runtime C++ API (1–2h)

**Subclass `TBackground`** into `TWibWobBackground` (in `app/wibwob_background.h/.cpp`):

```cpp
class TWibWobBackground : public TBackground {
public:
    TWibWobBackground(const TRect& bounds, char pattern, uchar fg, uchar bg) noexcept;
    void setTexture(char ch);
    void setColor(uchar fg, uchar bg);
    void draw() override;
private:
    uchar fgColor, bgColor;
};
```

`draw()` override builds `TDrawBuffer` with an explicit `TColorAttr` from `fgColor`/`bgColor` rather than relying on the palette lookup — this means it works regardless of the app palette state.

**Wire into `initDeskTop()`** — return a `TDeskTop` that uses `TWibWobBackground` instead of the default `TBackground`:

```cpp
TDeskTop* TTestPatternApp::initDeskTop(TRect r) {
    // ... (existing bounds trim) ...
    TDeskTop* desk = new TDeskTop(r, &TWibWobBackground::init);
    return desk;
}
```

Or simpler: after init, cast `deskTop->background` and replace it:
```cpp
// In TTestPatternApp constructor after base init:
auto bg = dynamic_cast<TBackground*>(deskTop->background);
// Remove old, insert new TWibWobBackground with same bounds
```

The replace-after-init path is messier (insertion order); subclassing via `TDeskInit` is cleaner.

**App-level setter**:

```cpp
void TTestPatternApp::setDesktopTexture(char ch);
void TTestPatternApp::setDesktopColor(uchar fg, uchar bg);
void TTestPatternApp::setDesktopPreset(const std::string& preset);
// preset = "black", "cga_blue", "cga_cyan", "shade1"…"shade3", etc.
```

### Phase 2 — API + MCP endpoints (30m)

In `tools/api_server/main.py`:

```
POST /desktop/texture   body: { "char": "░" }
POST /desktop/color     body: { "fg": 7, "bg": 1 }
POST /desktop/preset    body: { "preset": "cga_blue_shade2" }
GET  /desktop           returns: { "char": "▒", "fg": 7, "bg": 1, "preset": "cga_blue_shade2" }
```

IPC command → new `cmDesktopTexture`, `cmDesktopColor`, `cmDesktopPreset` in `command_registry.cpp`.

MCP tool: `set_desktop` with `preset` param (easy to call from Claude.ai).

### Phase 3 — Menu UI (30m, optional for spike)

Under `~V~iew` or `~D~esktop` submenu:

```
Desktop ▶  Texture ▶  [ ] Light (░)
                       [ ] Medium (▒)  ✓ 
                       [ ] Dark (▓)
                       [ ] Solid (█)
                       [ ] Empty ( )
           Colour  ▶  [ ] Black
                       [ ] Blue    ✓ 
                       [ ] Cyan
                       ...
```

Or a single dialog picker (later).

### Phase 4 — Workspace persistence (free, via E013 pattern)

Add `desktop_texture` and `desktop_color` to workspace save/restore JSON. Hook into existing `buildWorkspaceJson()` / `restoreWorkspace()`. The parity-check script will catch it if missed.

---

## Named Presets (MVP shortlist)

These cover the most aesthetically interesting combos:

| Preset name | Texture | BG | FG | Feel |
|---|---|---|---|---|
| `default` | ▒ | Blue (1) | Light Grey (7) | Classic TV default |
| `jet_black` | ` ` | Black (0) | Black (0) | Void / modern |
| `terminal` | ░ | Black (0) | Dark Grey (8) | CRT terminal |
| `cga_cyan` | ▒ | Cyan (3) | White (15) | Retro bright |
| `cga_green` | ░ | Black (0) | Bright Green (10) | Phosphor green |
| `noise` | % | Black (0) | Dark Grey (8) | Grungy |
| `white_paper` | ` ` | White (15) | White (15) | Blank canvas |
| `wibwob` | ▒ | Black (0) | Dark Grey (8) | W&W default vibe |

---

## Open Questions

- **TrueColor stretch goal**: tvision supports `TColorRGB` — is it worth exposing RGB hex input for bg? Probably yes for E005 theme wiring, but out of scope for this spike.
- **Per-session vs persisted preset**: Should the MCP tool change just the live session or also update a config file? Live-only is simpler for MVP.
- **Animation**: Could slowly cycle textures (░→▒→▓→░) on an idle timer — fun but definitely stretch.
- **Wib&Wob agency**: Should Scramble be able to call `set_desktop` unprompted (mood-driven)? Relates to E006 heartbeat/agency discussion — note for later.

---

## Acceptance Criteria (when this becomes a story)

- AC-1: `POST /desktop/preset` with `"preset": "jet_black"` changes desktop immediately in running TUI.
- AC-2: `GET /desktop` returns current texture char, fg, bg, and preset name.
- AC-3: At least 8 named presets available and tested.
- AC-4: Setting persists across workspace save/restore.
- AC-5: Menu submenu exposes at least texture and colour picks.
- AC-6: Parity check script passes after adding new workspace props.

---

## Effort Estimate

| Phase | Est |
|---|---|
| Phase 1 (C++ subclass + app setter) | 2h |
| Phase 2 (API + MCP) | 1h |
| Phase 3 (menu UI) | 1h |
| Phase 4 (workspace persistence) | 30m |
| **Total** | **~4.5h** |

Suitable for a single focused story under E005 (theme runtime wiring) or as a standalone mini-epic.
