# Edits Applied to Specification Documents

Date: 2026-02-23
Documents modified: 3
Total edits: 7

---

## FILE 1: ASCII_GALLERY_LAYOUT_SPEC.md

### Edit 1: Updated FR5 — Layout Persistence

**Location**: Section "## Requirements" → "#### FR5: Layout Persistence"

**Old text**:
```
#### FR5: Layout Persistence

- New command: `save_gallery_layout` — serializes current window arrangement by filename
- New command: `load_gallery_layout` — restores a named arrangement
- Supports curated "exhibition" layouts (e.g., "chaos-vs-order collection", "monster week", etc.)
```

**New text**:
```
#### FR5: Layout Persistence

**Approach**: Leverage existing `save_workspace` and `load_workspace` commands
- User arranges primers using `smart_gallery_arrange` or manual positioning
- Call `save_workspace` to serialize window layout (including positions, sizes, types)
- Call `load_workspace` to restore a named exhibition
- Supports curated "exhibition" layouts (e.g., "chaos-vs-order collection", "monster week", etc.)
- No need for custom gallery-specific serialization; workspace persistence handles it

**Optional enhancement**: Add metadata tags to workspace files (theme, curator notes, creation date) for better curation UX
```

---

### Edit 2: Added FR6 — Frameless Window Mode (NEW)

**Location**: After FR5, before "## Design Decisions"

**New section added**:
```
#### FR6: Frameless Window Mode (NEW)

**Requirement**: Display artwork without window chrome (borders, title bars)

This is critical for immersive gallery experiences. When viewing a delicate ASCII piece, the TUI window border is visual noise. Frameless mode eliminates it.

**Specification**:
- New window property: `frameless: bool` (default false)
- When true:
  - No title bar rendered
  - No border/frame visible
  - Content extends to window edges
  - Window still occupies the same x/y/width/height space (hit-box unchanged)

**API Usage**:
```json
POST /menu/command
{
  "command": "open_primer",
  "args": {
    "path": "reality-breaks-apart-abstract.txt",
    "frameless": true
  }
}
```

Or retrofit via:
```json
POST /tui/window/{window_id}
{
  "frameless": true
}
```

**Rationale**:
- Frameless maximizes visible content in a constrained canvas
- Creates "floating art" aesthetic — pieces appear to exist without container
- Enables seamless gallery walls where art flows edge-to-edge
- Respects the artist's composition; no frame interferes with it

**Usage in Smart Gallery**:
```json
{
  "command": "smart_gallery_arrange",
  "args": {
    "filenames": [...],
    "algorithm": "masonry",
    "frameless": true  // Apply frameless to all opened pieces
  }
}
```

**Visual Example** (with vs without):

WITH FRAME (current):
```
┌─ reality-breaks-apart-abstract.txt ──────────────────┐
│                                                        │
│  /\    /\                                             │
│ /  \  /  \       (ASCII art content)                  │
│                                                        │
└────────────────────────────────────────────────────────┘
```

FRAMELESS (new):
```
  /\    /\
 /  \  /  \       (ASCII art content)

(Piece appears to float on the canvas with no visual border)
```

**Implementation Notes**:
- Frame_player or similar renderer needs to support `frameless` flag
- Layout algorithm doesn't change; it still reserves the same space
- Hit-detection/focus still works normally (invisible frame)
- Optional: Add subtle shadow or glow behind frameless windows (depth cue)
```

---

### Edit 3: Updated Spike Plan — Restructured & Added Frameless Phase

**Location**: Section "## Spike Plan (This Sprint)"

**Old text**:
```
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
```

**New text**:
```
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
- [ ] Test with `frameless: true` flag

### Phase 4: Frameless Window Mode (1-2 hours)
- [ ] Add `frameless` property to window renderer
- [ ] Update `open_primer` to accept `frameless` arg
- [ ] Update `tui_move_window` to support setting `frameless` on existing windows
- [ ] Test visual rendering (no title/border, content edge-to-edge)
- [ ] Validate with smart_gallery_arrange + frameless combination

### Phase 5: Workspace Integration (30 minutes)
- [ ] Verify existing `save_workspace` / `load_workspace` work for gallery layouts
- [ ] Document best practices (e.g., "To save a gallery: run smart_arrange, then save_workspace")
- [ ] No new code needed; leverage existing commands
- [ ] Optional: Add metadata sidecar to workspace files (.json + .metadata.json) for curator notes

### Phase 6: Manual Fine-Tuning (1 hour, optional)
- [ ] Add `fit_to_content` and `preserve_aspect_ratio` flags to `tui_move_window`
- [ ] Test with user-driven manual resizing workflow
```

---

### Edit 4: Updated Success Criteria

**Location**: Section "## Success Criteria"

**Old text**:
```
## Success Criteria

- [ ] Metadata API operational for all 172 primers
- [ ] Masonry layout correctly arranges 5+ diverse primers without overlap
- [ ] Smart gallery command produces visually pleasing results (subjective; requires aesthetic review)
- [ ] No window exceeds canvas bounds
- [ ] Arrangement can be saved and restored accurately
- [ ] Performance: arranging 20+ primers completes in <500ms
```

**New text**:
```
## Success Criteria

- [ ] Metadata API operational for all 172 primers
- [ ] Masonry layout correctly arranges 5+ diverse primers without overlap
- [ ] Smart gallery command produces visually pleasing results (subjective; requires aesthetic review)
- [ ] No window exceeds canvas bounds
- [ ] Frameless mode renders artwork without title/border chrome
- [ ] Frameless windows integrate seamlessly with smart_gallery_arrange
- [ ] Arrangement can be saved and restored via existing save_workspace/load_workspace commands
- [ ] Performance: arranging 20+ primers completes in <500ms
```

---

## FILE 2: MCP_API_SURFACE.md

### Edit 5: Updated Available Commands Section

**Location**: Section "### Content & Rendering" → "#### `tui_menu_command`" → "Available commands"

**Old text**:
```
Available commands (relevant to gallery):
- `open_primer` — args: { path: "filename.txt" }
- `gallery_list` — args: {} (optional tab param)
- `open_gallery` — args: {}
```

**New text**:
```
Available commands (relevant to gallery):
- `open_primer` — args: { path: "filename.txt", frameless?: bool }
- `gallery_list` — args: {} (optional tab param)
- `open_gallery` — args: {}
- `save_workspace` — args: {} (saves current layout including all open primer windows)
- `load_workspace` — args: { path: "layout.json" } (restores saved gallery layouts)
```

---

### Edit 6: Added Frameless Window Mode Section & Reorganized

**Location**: After "### 2. Smart Gallery Arrangement", before "### 3. Layout Persistence"

**New section added**:
```
### 3. Frameless Window Mode (NEW)

**Enhancement to open_primer and tui_move_window**:

```
POST /menu/command
{
  "command": "open_primer",
  "args": {
    "path": "reality-breaks-apart-abstract.txt",
    "frameless": true
  }
}
```

Or apply to existing window:
```
POST /tui/window/{window_id}
{
  "frameless": true
}
```

**Implementation**:
- Add `frameless: bool` property to frame_player windows (default false)
- When true:
  - No title bar rendered
  - No border/box-drawing characters visible
  - Content extends edge-to-edge within the window bounds
  - Hit-box remains the same (invisible frame for focus/interaction)
- Minimal code: render path branches on this flag (~30 lines)

**Benefits**:
- Maximizes visible content on constrained canvas
- Creates "floating art" aesthetic (pieces appear without container)
- Enables seamless gallery walls where art flows edge-to-edge
- Integrates perfectly with smart_gallery_arrange for immersive experiences

**Complexity**: ~30–50 lines (render flag + conditional)
```

**Note**: This pushed the old "### 3. Layout Persistence" → "### 4. Layout Persistence"

---

### Edit 7: Updated Layout Persistence Section (Formerly #3, Now #4)

**Location**: "### 4. Layout Persistence (Reusing Existing Commands)"

**Old text**:
```
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
```

**New text**:
```
### 4. Layout Persistence (Reusing Existing Commands)

**Approach**: Leverage built-in `save_workspace` and `load_workspace` commands

```
POST /menu/command
{
  "command": "save_workspace",
  "args": {}
}

Response:
{
  "ok": true,
  "result": "Workspace saved to ~/.wibwob/workspaces/workspace-{timestamp}.json"
}
```

```
POST /menu/command
{
  "command": "load_workspace",
  "args": { "path": "~/.wibwob/workspaces/my-gallery.json" }
}

Response:
{
  "ok": true,
  "result": "Workspace restored; 5 windows repositioned"
}
```

**Implementation**:
- No new code needed; existing workspace commands handle it
- User workflow: arrange primers → call `smart_gallery_arrange` → call `save_workspace` → name the file descriptively
- Workspace JSON includes all window state (position, size, type, content path)
- Load any saved workspace to restore the exact gallery layout

**Optional enhancement**: Add metadata sidecar (.workspace.metadata.json) for curator notes, theme tags, creation date. Not required for MVP.
```

---

### Edit 8: Updated Implementation Effort Estimate Table

**Location**: Section "## Implementation Effort Estimate"

**Old text**:
```
| Feature | Effort | Blocking |
|---------|--------|----------|
| Primer metadata endpoint | 1-2 hours | No |
| Masonry algorithm | 1-2 hours | Yes (needs metadata) |
| Smart arrange command | 1 hour | No (depends on above) |
| Layout persistence | 1-2 hours | No |
| Enhanced move_window flags | 30 mins | No |
| **Total** | **5-9 hours** | — |
```

**New text**:
```
| Feature | Effort | Blocking |
|---------|--------|----------|
| Primer metadata endpoint | 1-2 hours | No |
| Masonry algorithm | 1-2 hours | Yes (needs metadata) |
| Smart arrange command | 1 hour | No (depends on above) |
| Frameless window mode | 0.5-1 hour | No |
| Layout persistence (reuse save_workspace) | 0 hours (existing) | No |
| Enhanced move_window flags | 30 mins | No |
| **Total** | **4.5-6.5 hours** | — |
```

---

## FILE 3: GALLERY_VISION.md

### Edit 9: Added Core Principle #6

**Location**: Section "### Core Principles"

**Old text**:
```
1. **Honour the original**: Respect the intrinsic width and height of each piece
2. **Breathing room**: Space between pieces; don't pack the canvas solid
3. **Intentionality**: Every placement should feel chosen, not default
4. **Play with perception**: Use negative space to create rhythm and visual interest
5. **Embrace strangeness**: A chaotic arrangement can be more beautiful than a perfect grid
```

**New text**:
```
1. **Honour the original**: Respect the intrinsic width and height of each piece
2. **Breathing room**: Space between pieces; don't pack the canvas solid
3. **Intentionality**: Every placement should feel chosen, not default
4. **Play with perception**: Use negative space to create rhythm and visual interest
5. **Embrace strangeness**: A chaotic arrangement can be more beautiful than a perfect grid
6. **No chrome**: Use frameless mode to eliminate window borders — art floats on canvas
```

---

### Edit 10: Added "Frameless Mode: The Missing Polish" Section

**Location**: After "### Mode 4: FLOW (MASONRY)", before "## Enhancement Ideas"

**New section added**:
```
---

## Frameless Mode: The Missing Polish

**The Problem**: Window frames (title bars, borders) are visual noise when you're trying to view beautiful ASCII art. They clutter the gallery and distract from the work.

**The Solution**: **Frameless mode**. Open artwork without any chrome — no title bar, no border, no box-drawing characters. The art floats directly on the canvas.

**Why It Matters**:
- Maximizes visible content in a constrained 320-column canvas
- Creates a "floating art" aesthetic — pieces appear to exist without a container
- Enables seamless gallery walls where art flows edge-to-edge, uninterrupted
- Respects the artist's composition; no frame interferes with the visual intent
- Transforms the gallery from "utility" to "immersive experience"

**Usage**:
```
open_primer(path="reality-breaks-apart-abstract.txt", frameless=true)
smart_gallery_arrange(algorithm="masonry", frameless=true)
```

**Visual Impact**:

With frame (distracting):
```
┌─ reality-breaks-apart-abstract.txt ────────────────┐
│ /\    /\                                           │
│/  \  /  \  (ASCII art content)                     │
│                                                    │
└────────────────────────────────────────────────────┘
```

Frameless (beautiful):
```
/\    /\
/  \  /  \  (ASCII art content, no box in the way)

(Piece appears to float; viewer focuses on the art, not the frame)
```

**Best Used With**:
- MUSEUM WALL layouts (frameless + sparse grid = gallery aesthetic)
- CHAOS MODE (frameless overlaps create layered depth without frame clutter)
- Paired with careful negative space (room for art to breathe)

**Optional Enhancement**: Very subtle shadow or glow behind frameless windows (depth cue), disabled by default.
```

---

## Summary

**Total edits**: 10 logical edits across 3 files

**Files**:
1. ASCII_GALLERY_LAYOUT_SPEC.md — 4 edits (FR5 rewrite, FR6 addition, Spike Plan restructure, Success Criteria update)
2. MCP_API_SURFACE.md — 3 edits (Commands update, Frameless section addition, Persistence section rewrite)
3. GALLERY_VISION.md — 2 edits (Core Principles addition, Frameless Mode section addition)

**Net change in scope**:
- Added: FR6 (Frameless), Phase 4 (Frameless implementation), 2 success criteria
- Removed: Custom save_gallery_layout / load_gallery_layout commands (now reuse save_workspace)
- Effort reduced: 5–9 hours → 4.5–6.5 hours (net savings: 0.5–4.5 hours)

All edits maintain consistency across documents and are production-ready for the `.planning` epic folder.
