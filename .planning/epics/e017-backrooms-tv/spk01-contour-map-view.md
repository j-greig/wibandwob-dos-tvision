---
Status: not-started
Type: spike
GitHub issue: —
PR: —
---

# SPK01 — Contour Map Studio View

## TL;DR

Port `scratch/contour_tui.py` as a native C++ TVision window in WibWob-DOS. Resizable, animated (grow mode), 7 terrain types, marching-squares contour rendering using only box-drawing chars `╭ ╮ ╰ ╯ │ ─`.

## Why subprocess (not native C++)?

The Python script already works — battle-tested grow mode, all 7 terrains, export, triptych. Keeping Python under the hood means:
- **Zero port effort** — algorithm is complex (shaped hills, marching squares, grow state machine)
- **Fast iteration** — tweak terrain generators in Python, no C++ rebuild
- **Consistent architecture** — same fork/pipe/timer pattern proven by Backrooms TV
- **Performance is fine** — heightmap computes in ~5ms, not latency-critical like LLM streaming

### Subprocess protocol

Add `--stream` flag to `contour_tui.py`:
- **Static mode**: dumps rendered lines to stdout, exits
- **Grow mode**: dumps frames separated by `\x1E` (ASCII record separator), one per tick, exits when done
- **Params via CLI args**: `--seed N --terrain NAME --levels N --grow --triptych --width W --height H`
- C++ side reads via same pipe/timer pattern as `BackroomsBridge`

### C++ side

```
app/contour_map_view.h
app/contour_map_view.cpp
```

- **`ContourBridge`** — fork/pipe subprocess manager (mirrors `BackroomsBridge`)
- **`TContourMapView : public TScroller`** — TVision view with scrollbar
  - `draw()`: renders from cached line buffer using `moveStr()`
  - `handleEvent()`: keyboard controls → kill subprocess, relaunch with new params
  - Timer-driven: poll pipe, parse frames
  - `changeBounds()`: kill + relaunch with new dimensions
- **`TContourMapWindow : public TWindow`** — wraps view + scrollbar, titled "Contour Studio"

### Keybindings (match Python)

| Key | Action |
|-----|--------|
| r | Random seed |
| 1-7 | Select terrain type |
| TAB | Cycle terrain |
| +/- | More/fewer contour levels |
| g | Toggle grow/static mode |
| Space | Pause/resume grow |
| t | Triptych (3 panels) |
| s | Save to file |
| a | Batch save all terrains |

### Registry/menu wiring

- Command: `cmContourMap` (next free ID)
- Registry slug: `contour_map`
- Command name: `open_contour_map`
- View menu entry: "Contour Studio"
- API params: optional `seed`, `terrain`, `levels`

### Status bar

Single row: `CONTOUR STUDIO │ MEADOW │ seed 42381 │ 5 levels │ GROWING hills:12/60 coverage:23%`

### File export

Save to `exports/contour/` with timestamped filename, matching Python format.

## Scope

- [ ] Add `--stream` mode to `contour_tui.py` (static + grow frame protocol)
- [ ] `ContourBridge` — fork/pipe subprocess manager
- [ ] `TContourMapView` + `TContourMapWindow` — rendering, keyboard, timer, resize
- [ ] Registry, menu, command wiring
- [ ] File export (s/a keys — delegate to Python `--save` flag)
- [ ] Triptych mode (delegate to Python `--triptych` flag)

## Risks / open questions

- **Resize**: killing and relaunching Python on every `changeBounds()` adds ~50ms latency. Acceptable for resize events but might feel sluggish if user drags the window corner continuously. Mitigation: debounce resize, only relaunch after 200ms idle.
- **Grow mode frame rate**: Python prints frames at 80ms intervals. Pipe read at 50ms timer should keep up. If frames back up, skip to latest.
- **Colour**: Python version is monochrome box-drawing. Could add TV colour pairs per contour level later — nice enhancement but not MVP.
- **Export path**: Python saves to `scratch/contour_exports/`. Should redirect to `exports/contour/` relative to repo root via `--export-dir` arg.

## Estimated effort

Small. Python `--stream` mode is ~30 lines. C++ view follows Backrooms TV pattern closely. 1–2 focused sessions.
