# MCP API Surface Analysis — WibWob TUI

## Current Tools & Their Signatures

### Window Management

#### `tui_create_window`
```
Input: { type, title?, x?, y?, width?, height?, props? }
Output: { window_id }
Notes: Creates any window type. No validation of dimensions.
```

#### `tui_move_window`
```
Input: { window_id, x?, y?, width?, height? }
Output: { ok }
Notes: Only accepts partial updates (send only fields you want to change).
```

#### `tui_get_state`
```
Input: {}
Output: {
  canvas: { width, height },
  windows: [{ id, type, title, x, y, width, height }],
  pattern_mode, theme
}
Notes: Read-only. Reflects current layout state.
```

### Content & Rendering

#### `tui_menu_command`
```
Input: { command: string, args: object }
Output: { ok, result? }
Notes: Generic dispatcher. Commands are discovered via `tui_list_commands`.
```

Available commands (relevant to gallery):
- `open_primer` — args: { path: "filename.txt" }
- `gallery_list` — args: {} (optional tab param)
- `open_gallery` — args: {}

#### `tui_screenshot`
```
Input: {}
Output: (image file written; can be read via Read tool)
Notes: Captures entire canvas as text/image. Useful for visual verification.
```

### Current Gaps for Gallery Layout System

1. **No primer metadata endpoint**
   - Cannot query dimensions of artwork before opening
   - Would need to either:
     a. Add new command `primer_metadata { filename }`
     b. Extend `/api` surface (if backend REST API exposed)

2. **No window introspection per-window**
   - Can't ask "what is the natural width of content in window X?"
   - Would need frame_player to expose this, or screenshot + parse

3. **No batch window operations**
   - Can only move one window at a time
   - Applying Masonry arrangement = N sequential `tui_move_window` calls
   - Could batch, but not critical (network overhead negligible in local TUI)

4. **No layout serialization**
   - No command to export/import window arrangements
   - Would need to add `save_gallery_layout` / `load_gallery_layout` commands

---

## Proposed API Extensions

### 1. Primer Metadata Query

**Command**: `primer_metadata`
```
POST /menu/command
{
  "command": "primer_metadata",
  "args": { "filename": "reality-breaks-apart-abstract.txt" }
}

Response:
{
  "ok": true,
  "result": {
    "filename": "reality-breaks-apart-abstract.txt",
    "width": 78,
    "height": 42,
    "aspect_ratio": 1.86,
    "line_count": 42,
    "max_line_width": 78
  }
}
```

**Implementation**:
- Backend parses primer files on first load, caches metadata
- Metadata file: `.metadata.json` per primer or single index
- Response time: <10ms (cached)

---

### 2. Smart Gallery Arrangement

**Command**: `smart_gallery_arrange`
```
POST /menu/command
{
  "command": "smart_gallery_arrange",
  "args": {
    "filenames": ["file1.txt", "file2.txt", "file3.txt"],
    "algorithm": "masonry",
    "padding": 2,
    "preview": false
  }
}

Response (preview=false):
{
  "ok": true,
  "result": "arranged"
}

Response (preview=true):
{
  "ok": true,
  "result": {
    "arrangement": [
      { "filename": "file1.txt", "x": 0, "y": 0, "width": 78, "height": 42 },
      { "filename": "file2.txt", "x": 80, "y": 0, "width": 68, "height": 35 }
    ],
    "utilization": 0.87,
    "overlaps": 0
  }
}
```

**Implementation**:
- New TUI command handler
- Calls `primer_metadata` for each file
- Runs Masonry algorithm (Python or C++ impl)
- Calls `tui_move_window` for each placement
- Returns placement report

**Complexity**: ~200 lines of code (algorithm + integration)

---

### 3. Layout Persistence

**Command**: `save_gallery_layout`
```
POST /menu/command
{
  "command": "save_gallery_layout",
  "args": { "name": "my-curated-show" }
}

Response:
{
  "ok": true,
  "result": {
    "layout_id": "my-curated-show",
    "windows_saved": 5,
    "path": "~/.wibwob/layouts/my-curated-show.json"
  }
}
```

**Command**: `load_gallery_layout`
```
POST /menu/command
{
  "command": "load_gallery_layout",
  "args": { "name": "my-curated-show" }
}

Response:
{
  "ok": true,
  "result": "layout restored; 5 windows repositioned"
}
```

**Implementation**:
- Serialize `tui_get_state()` windows by filename
- Write JSON to `~/.wibwob/layouts/{name}.json`
- On load, close current primer windows, re-open, apply positions

---

### 4. Enhanced tui_move_window (Optional)

```
POST /tui/window/{window_id}
{
  "x": 0,
  "y": 0,
  "width": 78,
  "height": 42,
  "fit_to_content": true,
  "preserve_aspect_ratio": true
}
```

**Parameters**:
- `fit_to_content`: If true, snap width/height to nearest sensible size for the window's content
- `preserve_aspect_ratio`: If resizing, maintain aspect ratio by adjusting the non-specified dimension [zilla: not posssible with ascii art is it, thinka bout it!]

**Implementation**: ~50 lines (logic in window manager)

---

## Architecture Diagram

```
User calls smart_gallery_arrange
         ↓
   tui_menu_command dispatcher
         ↓
   [new] GalleryCommand handler
         ├─→ primer_metadata (x N filenames)
         │    └─→ cached metadata index
         ├─→ Masonry algorithm
         │    └─→ calculates placements
         └─→ apply via tui_move_window (batch)
         ↓
   Windows repositioned on canvas
         ↓
   User sees beautifully arranged gallery
```

---

## Implementation Effort Estimate

| Feature | Effort | Blocking |
|---------|--------|----------|
| Primer metadata endpoint | 1-2 hours | No |
| Masonry algorithm | 1-2 hours | Yes (needs metadata) |
| Smart arrange command | 1 hour | No (depends on above) |
| Layout persistence | 1-2 hours | No |
| Enhanced move_window flags | 30 mins | No |
| **Total** | **5-9 hours** | — |

---

## Tool Limitations & Workarounds

### Limitation 1: Frame Player Windows Don't Expose Content Dimensions

**Problem**: We can't ask a frame_player what its intrinsic content width is.

**Workaround A** (Recommended): Metadata endpoint reads source files
- Pro: Reliable, fast, decoupled
- Con: Requires backend work

**Workaround B**: Screenshot + OCR/Analysis
- Pro: Works with existing API surface
- Con: Fragile, slow, depends on renderer output

### Limitation 2: No Batch Window Operations

**Problem**: Masonry of 20 pieces = 20 sequential API calls.

**Workaround**: Add batch command
```
{
  "command": "batch_move_windows",
  "args": {
    "operations": [
      { "window_id": "w1", "x": 0, "y": 0, "width": 78, "height": 42 },
      { "window_id": "w2", "x": 80, "y": 0, "width": 68, "height": 35 }
    ]
  }
}
```

**Current Impact**: Negligible (TUI is local, latency <1ms per call).

### Limitation 3: Canvas Dimensions Are Fixed at Runtime

**Problem**: Can't detect user resizing canvas.

**Workaround**:
- Call `tui_get_state` periodically or on demand
- Re-run layout if canvas dimensions change
- User hits a "re-arrange" hotkey or command

---

## Recommended Implementation Order

1. **Metadata endpoint** — foundation, enables everything else
2. **Masonry algorithm** — core layout logic, testable independently
3. **Smart arrange command** — user-facing feature, integrates 1+2
4. **Layout persistence** — nice-to-have, low complexity - [zilla: we have save workspace already use that!]
5. **Enhanced move_window** — polish, optional

---

## Testing Strategy

### Unit Tests
- Metadata parsing: 10 sample primers
- Masonry algorithm: 5 layout scenarios (narrow, wide, mixed, tall, short)
- No overlaps, no out-of-bounds placements

### Integration Tests
- Open 5 primers → smart_arrange → verify positions via tui_get_state
- Save layout → close primers → load layout → verify restoration
- Canvas resize → re-arrange → verify fit

### Visual Inspection
- Screenshot after each arrangement
- Manual aesthetic review (Wib: does it look intentional and beautiful?)

---

## Edge Cases to Handle

1. **Piece wider than canvas**: Scale down or crop?
   - Recommendation: Keep aspect ratio, shrink to fit, show warning

2. **Piece taller than canvas**:
   - Recommendation: Show warning, place anyway (user can scroll or resize canvas)

3. **Canvas with no room left**:
   - Recommendation: Cascade instead of Masonry

4. **Empty filenames or duplicates**:
   - Validation: Filter; deduplicate by filename

5. **Metadata file corrupted**:
   - Fallback: Re-parse source file, cache result

---

## Appendix: Current Tool JSON Examples

### tui_get_state response (current)
```json
{
  "canvas": { "width": 320, "height": 78 },
  "windows": [
    {
      "id": "w1",
      "type": "wibwob",
      "title": "Wib&Wob Chat 1",
      "x": 160,
      "y": 0,
      "width": 160,
      "height": 19
    },
    {
      "id": "w3",
      "type": "frame_player",
      "title": "",
      "x": 0,
      "y": 0,
      "width": 160,
      "height": 25
    }
  ],
  "pattern_mode": "continuous",
  "theme_mode": "light"
}
```

### tui_move_window call (current)
```json
{
  "windowId": "w3",
  "x": 0,
  "y": 0,
  "width": 78,
  "height": 42
}
```

### tui_menu_command call (current)
```json
{
  "command": "open_primer",
  "args": { "path": "reality-breaks-apart-abstract.txt" }
}
```

---

**Document Status**: Ready for architectural review & sprint planning
