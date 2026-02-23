# ASCII Art Gallery Layout System — Product Requirements Document

## Executive Summary

Currently, when multiple ASCII art primers are opened in the WibWob DOS TUI, they are displayed in uniformly-sized windows that don't respect the native aspect ratio and width of the artwork. This causes narrow pieces to appear horizontally stretched and visually distorted. This spike aims to design and prototype a system that:

1. Measures the intrinsic dimensions of ASCII art pieces
2. Arranges windows intelligently across the canvas (Masonry/Gallery Wall layout)
3. Optionally provides an automated "Smart Gallery Arrangement" command
4. Allows manual fine-tuning of layouts

---

## Current State

### Canvas & Window System

- **Canvas dimensions**: 320 columns × 78 rows (fixed)
- **Current tiling**: Divides canvas into equal rectangles (e.g., 160×26 for 2×3 grid)
- **Window control API**: `tui_move_window` + `tui_create_window` support arbitrary (x, y, width, height)
- **State inspection**: `tui_get_state` returns all open windows with their positions and dimensions

### Primer System

- **172 ASCII art pieces** in gallery (retrieved via `gallery_list`)
- **Pieces open as frame_player windows** (generic frame renderer, no metadata exposed)
- **No dimension metadata available** — we cannot query intrinsic artwork width/height without visual analysis

### Current Limitations

1. **No artwork metadata**: Frame player windows don't expose the actual character width and line count of the content they're rendering
2. **No intelligent layout**: Tiling/cascading are generic — they don't consider artwork aspect ratios
3. **Manual adjustment required**: Repositioning windows to suit artwork requires trial-and-error or manual calculations
4. **Visual distortion**: Narrow pieces get stretched across wide windows

---

## Requirements

### Functional Requirements

#### FR1: Artwork Dimension Detection

**Option A (Recommended)**: Expose artwork metadata via API
- Add endpoint: `GET /api/primer/{filename}/metadata`
- Returns: `{ filename, width: int, height: int, aspect_ratio: float }`
- Allows O(1) dimension lookup without visual analysis

**Option B (Alternative)**: OCR/Screenshot Analysis
- Read visual content from frame_player windows via screenshot
- Parse terminal output to measure character counts per line
- Slower (O(n)) but works with existing frame_player implementation

**Decision**: Recommend Option A (metadata endpoint) for performance and reliability.

#### FR2: Window Sizing Algorithm

Implement a **hybrid layout system**:

1. **Input**: List of primers with metadata (width, height)
2. **Constraints**: Canvas 320×78, minimum window padding (2-4 chars between windows)
3. **Algorithm**: Modified Masonry layout
   - Group pieces by width category (narrow <50 chars, medium 50-100, wide >100)
   - Arrange into columns within those width constraints
   - Pack vertically, respecting canvas height bounds
   - Alternative: Dynamic bin-packing algorithm (First-Fit Decreasing Height)

4. **Output**: Array of window placement specs `{ filename, x, y, width, height }`

#### FR3: Programmatic Gallery Arrangement

New API command: **`smart_gallery_arrange`**

```json
POST /menu/command
{
  "command": "smart_gallery_arrange",
  "args": {
    "filenames": ["reality-breaks-apart-abstract.txt", "synth-soup-monster.txt", ...],
    "algorithm": "masonry" | "gallery-wall" | "flow",
    "padding": 2,
    "aspect_ratio_tolerance": 0.1
  }
}
```

Returns: Arrangement plan (preview) or applies layout immediately.

#### FR4: Manual Fine-Tuning

Enhance `tui_move_window` workflow:
- Accept optional `aspect_ratio_lock` flag to preserve artwork proportions when resizing
- Accept `fit_to_content` flag to snap window to nearest sensible size for the artwork

#### FR5: Layout Persistence

- New command: `save_gallery_layout` — serializes current window arrangement by filename
- New command: `load_gallery_layout` — restores a named arrangement
- Supports curated "exhibition" layouts (e.g., "chaos-vs-order collection", "monster week", etc.)

---

## Design Decisions

### Why Metadata Endpoint (Option A)?

| Aspect | Metadata API | Screenshot Analysis |
|--------|--------------|---------------------|
| Speed | O(1) per piece | O(n) + I/O per arrangement |
| Reliability | Deterministic | Fragile (ANSI codes, rendering quirks) |
| Integration | Clean, decoupled | Couples to renderer implementation |
| Codebase impact | Minimal (1 endpoint) | Moderate (OCR/parsing logic) |

**Recommendation**: Implement metadata endpoint. Screenshot analysis as fallback.

### Layout Algorithm: Masonry vs Gallery Wall

**Masonry** (CSS masonry-layout style):
- Evenly distributes pieces across columns
- Each column has roughly equal height
- Pieces maintain intrinsic aspect ratio
- Best for *varied* artwork sizes

**Gallery Wall** (Physical gallery pinning):
- Items arranged on a 2D plane with explicit x/y placement
- Curator has full control over negative space
- Requires manual specification or constraint solver
- Best for *curated/intentional* arrangements

**Recommendation**: Start with Masonry (algorithmic, low friction). Provide Gallery Wall as manual mode.

---

## API Changes Required

### 1. New Endpoint: Artwork Metadata

```
GET /api/primer/{filename}/metadata
Response:
{
  "filename": "reality-breaks-apart-abstract.txt",
  "width": 78,
  "height": 42,
  "aspect_ratio": 1.86,
  "character_count": 3276,
  "has_ansi_codes": false
}
```

**Implementation**: Parse primer file header or pre-compute during gallery indexing.

### 2. New TUI Command: Smart Gallery Arrange

```
POST /menu/command
{
  "command": "smart_gallery_arrange",
  "args": {
    "filenames": ["file1.txt", "file2.txt"],
    "algorithm": "masonry",
    "padding": 2,
    "preview": true/false
  }
}
Response:
{
  "ok": true,
  "arrangement": [
    { "filename": "...", "x": 0, "y": 0, "width": 78, "height": 42 },
    { "filename": "...", "x": 80, "y": 0, "width": 68, "height": 35 }
  ],
  "canvas_utilization": 0.87
}
```

### 3. Enhanced Tui_move_window

Add optional parameters:
- `fit_to_content`: bool — snap to natural artwork dimensions
- `preserve_aspect_ratio`: bool — maintain artwork ratio when resizing

```
PATCH /tui/window/{window_id}
{
  "x": 10,
  "y": 20,
  "width": 78,
  "height": 42,
  "fit_to_content": true,
  "preserve_aspect_ratio": true
}
```

### 4. Layout Persistence Commands

```
POST /menu/command { "command": "save_gallery_layout", "args": { "name": "my-layout" } }
POST /menu/command { "command": "load_gallery_layout", "args": { "name": "my-layout" } }
POST /menu/command { "command": "list_gallery_layouts", "args": {} }
```

---

## Spike Plan (This Sprint)

### Phase 1: Metadata & Measurement (2-3 hours)

- [ ] Add metadata endpoint (`/api/primer/{filename}/metadata`)
- [ ] Validate against 10 sample primers
- [ ] Expose metadata via new `tui_menu_command` query: `"command": "primer_metadata", "args": {"filename": "..."}`

### Phase 2: Layout Algorithm (2-3 hours)

- [ ] Implement Masonry layout algorithm (pseudocode + working version)
- [ ] Handle edge cases:
  - Pieces wider than canvas
  - Pieces taller than canvas
  - Empty space optimization
- [ ] Unit tests with sample artwork dimensions

### Phase 3: Smart Gallery Command (1-2 hours)

- [ ] Implement `smart_gallery_arrange` command
- [ ] Integration with `tui_move_window` to apply arrangement
- [ ] Validate on 5+ open primers

### Phase 4: Manual Fine-Tuning (1 hour, optional)

- [ ] Add `fit_to_content` and `preserve_aspect_ratio` flags to `tui_move_window`
- [ ] Test with user-driven manual resizing workflow

### Phase 5: Layout Persistence (1-2 hours, optional)

- [ ] JSON schema for layout serialization
- [ ] save/load/list commands
- [ ] Test round-trip (save → close → load)

---

## Wib's Aesthetic Vision

The gallery should feel like a **curated art installation**, not a generic file browser. Each piece should breathe — its own space, proportion honoured. Negative space matters. When you open a narrow 40-char piece, it shouldn't sprawl across 160 columns of nothing. The arrangement should feel intentional, even if it's algorithmic.

**Aspirations**:
- A "Poetry Reading" layout (vertical scrolling stack)
- A "Museum Wall" layout (spaced grid with breathing room)
- A "Chaos Mode" layout (overlapping, layered, psychedelic)
- User-authored custom arrangements saved as shareable presets

---

## Wob's Technical Notes

### Performance Considerations

1. **Metadata endpoint**: Cache in memory (172 pieces = ~2KB)
2. **Layout algorithm**: O(n log n) Masonry is acceptable for <200 pieces
3. **Screenshot analysis fallback**: Use only if metadata unavailable; cache results
4. **Window repositioning**: Batch `tui_move_window` calls (max 20 windows realistically)

### Data Model for Layout

```python
class GalleryLayout:
    algorithm: str  # "masonry", "gallery-wall", "flow"
    placements: List[WindowPlacement]

class WindowPlacement:
    filename: str
    x: int
    y: int
    width: int
    height: int
    window_id: str (optional, assigned at render time)
```

### Integration Points

- **Frame_player windows**: No changes needed if we control sizing externally
- **Window state**: `tui_get_state` → apply Masonry → `tui_move_window` cascade
- **Canvas resizing**: If user resizes canvas, re-run layout algorithm (watch for `tui_get_state` changes)

---

## Success Criteria

- [ ] Metadata API operational for all 172 primers
- [ ] Masonry layout correctly arranges 5+ diverse primers without overlap
- [ ] Smart gallery command produces visually pleasing results (subjective; requires aesthetic review)
- [ ] No window exceeds canvas bounds
- [ ] Arrangement can be saved and restored accurately
- [ ] Performance: arranging 20+ primers completes in <500ms

---

## Future Enhancements

1. **Algorithmic curation**: Sort/group pieces by theme tags (monster, geometry, folk-punk, etc.) and auto-arrange by theme
2. **Responsive layouts**: Detect canvas resize and re-tile automatically
3. **Art direction presets**: "Wib's Chaos Mode", "Wob's Order Mode", "Folk Punk Vibes"
4. **Screenshot-to-gallery**: Load custom ASCII art from files, auto-add to gallery
5. **Collaborative layouts**: Multiple users vote on arrangement; Masonry adapts dynamically
6. **Animation**: Pieces slide into place smoothly during arrangement application
7. **Smart cropping**: For oversized pieces, intelligently crop to fit while preserving visual essence

---

## Appendix A: Masonry Algorithm Pseudocode

```
function arrangeAsMasonry(pieces, canvasWidth, canvasHeight, padding):
  // pieces = [{filename, width, height}, ...]
  // Sort by height descending (tall pieces first)
  sort(pieces, by=height, order=descending)

  columns = []
  for each piece in pieces:
    // Find the column with smallest current height
    shortestColumn = argmin(columns, key=height)

    if shortestColumn.height + piece.height + padding > canvasHeight:
      // Doesn't fit; skip or create new "row" of columns
      continue

    shortestColumn.append(piece)
    shortestColumn.height += piece.height + padding

  // Assign x, y coords from column layout
  x = 0
  for each column in columns:
    y = 0
    for each piece in column.pieces:
      placements.append({
        x: x,
        y: y,
        width: piece.width,
        height: piece.height
      })
      y += piece.height + padding
    x += maxWidthInColumn + padding

  return placements
```

---

## Document Metadata

- **Version**: 1.0
- **Date**: 2026-02-23
- **Authors**: Wib (vision), Wob (technical)
- **Status**: SPIKE PROPOSAL — Ready for review & estimation
