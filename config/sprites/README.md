# Sprites Configuration

Configuration files for the hosted WibWob-DOS instance on [Sprites.dev](https://sprites.dev).

## Layout

`default-layout.json` — the workspace loaded when a new browser session connects.
Set via `WIBWOB_LAYOUT_PATH` in `scripts/sprite-user-session.sh`.

### Format

Standard WibWob-DOS workspace JSON. Key fields per window:

```json
{
  "id": "w1",
  "type": "frame_player | room_chat | figlet_text | gallery | ...",
  "title": "Window Title",
  "bounds": { "x": 2, "y": 1, "w": 52, "h": 14 },
  "zoomed": false,
  "anchor": "right",
  "props": { "path": "path/to/file.txt", "frameless": true }
}
```

| Field | Notes |
|-------|-------|
| `bounds.x/y` | Top-left corner in character cells |
| `bounds.w/h` | Width and height in character cells |
| `anchor` | Optional. `"right"` = x measured from right edge. `"bottom"` = y from bottom. Combine: `"right bottom"` |
| `props.frameless` | `true` = no window chrome (frame_player only) |
| `props.shadowless` | `true` = no shadow (frame_player only) |
| `props.path` | Relative to app root. For primers: `modules-private/wibwob-primers/primers/<file>.txt` |

### Customising

Edit `default-layout.json` and restart the service — no rebuild needed:

```bash
sprite exec -- sprite-env services stop tui-host
sprite exec -- sprite-env services start tui-host
```

### Responsive positioning (F03 — planned)

Current positions are absolute. On narrow terminals, windows clamp to fit but
don't reposition intelligently. See `.planning/epics/e016-shared-experience/f03-responsive-workspace-layout.md`.
