---
name: wibwobdos
description: Build, run, and operate WibWobDOS (C++ tvision TUI + FastAPI) in Docker. Screenshots, cross-platform C++ debugging, ARM64 Linux builds, container health.
---

# WibWobDOS Operations

C++ Turbo Vision TUI app + Python FastAPI server, running headless in Docker on ARM64 Linux.

## Screenshot

```bash
scripts/screenshot.sh                  # stdout (pipeable)
scripts/screenshot.sh -o file.txt      # save to file
scripts/screenshot.sh -f ansi          # ANSI color output
scripts/screenshot.sh -f ansi | convert # pipe to renderer
```

## Build & run

```bash
make up-real                          # real C++ backend
make up                               # mock (default)
make up-real && make provision && make deploy && make test   # full gate
```

## Quick health check

```bash
docker compose exec substrate bash -c 'curl -sf $WIBWOBDOS_URL/health'
docker compose logs wibwobdos         # startup log
```

## Module layout (post-E012)

`test_pattern_app.cpp` is being progressively extracted. New shared modules — include these
instead of adding code back into the god-object:

| Path | Contents |
|---|---|
| `app/core/json_utils.h` | `inline json_escape(s)` — use everywhere, do not re-implement |
| `app/ui/ui_helpers.h` | `makeStringCollection(vector<string>)` — builds `TStringCollection*` |
| `app/windows/frame_animation_window.h/.cpp` | `TFrameAnimationWindow` — extracted from `test_pattern_app.cpp` |
| `app/paint/paint_wwp_codec.h/.cpp` | `.wwp` JSON codec: `buildWwpJson`, `saveWwpFile`, `loadWwpFromString` |
| `app/paint/` | Paint canvas, tools, palette, status — already well-organised |

**Known pre-existing build issue:** `paint_tui` target fails to link (missing figlet symbols).
This predates E012. Build `test_pattern` target only — it is the main binary.

## C++ edit rules

**Always build after editing C++ files.** Do not report success without a clean build.

```bash
cmake --build ./build --target test_pattern 2>&1 | tail -5
```

If there are errors, fix them before proceeding. Most common issues:
- **`Uses_*` macros in new headers** — any new `.h` that includes `<tvision/tv.h>` MUST define
  the relevant `Uses_T*` macros immediately before that include, e.g.:
  ```cpp
  #define Uses_TWindow
  #define Uses_TRect
  #define Uses_TStringCollection
  #include <tvision/tv.h>
  ```
  Forgetting these causes "unknown type name" / "out-of-line definition does not match" errors.
  `Uses_TWindowInit` is not a valid macro — it comes for free with `Uses_TWindow`.
- Wrong `TMenuItem` constructor overload (submenu vs command)
- `TGroup::current` is a member, not a method
- Never forward-declare tvision types inside your own namespace
- Frame Z-order: `insertBefore(frame, nullptr)` = append as last/top child (matches TWindow ctor). `insert(frame)` = front = wrong.
- Frameless windows: if a child view covers 100% of the window, it must include parent chrome commands (Show Frame, Shadow, etc.) in its own right-click menu — otherwise those commands become unreachable.

## Turbo Vision responsive layout

### growMode flags (from tvision/views.h)

| Flag | Meaning |
|------|---------|
| `gfGrowLoX` | left edge tracks owner's right (moves right on grow) |
| `gfGrowHiX` | right edge tracks owner's right |
| `gfGrowLoY` | top edge tracks owner's bottom (moves down on grow) |
| `gfGrowHiY` | bottom edge tracks owner's bottom |
| `gfGrowAll` | all four — full stretch both axes |
| `gfGrowRel` | proportional rather than absolute delta |

Additive delta model: each flag adds `delta.x`/`delta.y` to the corresponding edge. No flag = edge stays fixed relative to owner's top-left.

### Common layout: 3-col + status bar

| Panel | growMode | Rationale |
|-------|----------|-----------|
| tools (left, fixed-width) | `gfGrowHiY` | fixed left/width, stretches down |
| canvas (centre, stretchy) | `gfGrowHiX \| gfGrowHiY` | left edge fixed, right/bottom track owner |
| palette (right, fixed-width) | `gfGrowLoX \| gfGrowHiX \| gfGrowHiY` | right-anchored fixed width, stretches down |
| status (bottom, full-width) | `gfGrowHiX \| gfGrowLoY \| gfGrowHiY` | full width, bottom-anchored, fixed height |

`gfGrowLoX | gfGrowHiX` = both edges track owner's right → constant width, slides right. Correct for right-anchored fixed-width panels.

### Gotcha: partial draw garbage

When `size.y` increases on resize, `draw()` must cover ALL rows up to `size.y`. If you only write fixed rows, garbage from previous content persists below. Fix: always blank-fill remaining rows.

### Better approach: changeBounds() override

For complex layouts, override `changeBounds()` in TWindow and manually compute child bounds:

```cpp
void MyWindow::changeBounds(const TRect& bounds) {
    TWindow::changeBounds(bounds);
    TRect client = getExtent();
    client.grow(-1, -1);
    int toolsW = 16, palW = 20;
    toolPanel->changeBounds(TRect(client.a.x, client.a.y,
        std::min(client.a.x + toolsW, client.b.x), client.b.y - 1));
    // ... etc for each child
}
```

Requires keeping pointers to child views. More code, guaranteed correct. Use when growMode flags aren't enough.

### Canvas buffer vs view size

Canvas buffer is allocated at construction. On resize, `size` grows but buffer stays fixed. `draw()` must handle `size > buffer` with fill. For true resize, reallocate buffer in `changeBounds()` and copy existing cells.

## References

- [Cross-platform C++ build guide](references/cross-platform-cpp.md) — Linux deps, transitive include gotchas, CMake linking
- [Docker operations](references/docker-ops.md) — startup sequence, SIGWINCH fix, troubleshooting table
- [Screenshot workflows](references/screenshots.md) — piping patterns, formats, full open-windows-and-capture workflow
- [Integrating vendor views](references/integrating-vendor-views.md) — 14-surface checklist, CMake gotchas, tvterm lessons learned

## Key files

| File | Purpose |
|---|---|
| `scripts/screenshot.sh` | Host-side screenshot capture (in skill dir) |
| `scripts/start-wibwobdos.sh` | Container entrypoint (build + TUI + API) |
| `Dockerfile.wibwobdos` | Real mode image |
| `docker-compose.real.yml` | Real mode override |
| `skills/wibwobdos-api/SKILL.md` | Symbient-facing API skill (separate) |
