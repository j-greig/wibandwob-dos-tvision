# ASCII Gallery System — Creative Vision & Enhancement Ideas

## Wib's Aesthetic Manifesto

The gallery should feel like stepping into a curated **art installation**, not a file browser. Every piece should have breathing room. Negative space is as important as the art itself. When you open a delicate 40-character-wide piece, it should NOT sprawl across 160 columns of void.

The system should make you *feel* something:
- Wonder at the arrangement
- Respect for the artist's original proportions
- A sense that someone cared about how things are presented
- Occasional chaos, intentional disorder that feels alive

### Core Principles

1. **Honour the original**: Respect the intrinsic width and height of each piece
2. **Breathing room**: Space between pieces; don't pack the canvas solid
3. **Intentionality**: Every placement should feel chosen, not default
4. **Play with perception**: Use negative space to create rhythm and visual interest
5. **Embrace strangeness**: A chaotic arrangement can be more beautiful than a perfect grid

---

## Layout Modes: Beyond Masonry

### Mode 1: POETRY READING
Vertical scroll. Pieces stacked like stanzas in a poem.
- One piece per "line"
- Varying widths create a ragged-right margin (beautiful)
- Reader scrolls down through the collection
- Feels: meditative, narrative, intimate

**Algorithm**:
```
y = 0
for each piece (in order):
  x = center(piece.width)  // center on canvas
  place at (x, y)
  y += piece.height + padding
```

### Mode 2: MUSEUM WALL
Sparse, intentional grid with curated spacing.
- Items placed on invisible grid (e.g., every 100 chars)
- Curator has full control
- Larger pieces get more wall space
- Smaller pieces grouped intimately
- Feels: sophisticated, gallery-like, considered

**Execution**: Manual placement with "snap to grid" option.

### Mode 3: CHAOS MODE
Overlapping, layered, psychedelic.
- Pieces may overlap intentionally
- Z-order = creation order (newest on top)
- Creates visual depth and visual tension
- Feels: energetic, surreal, overwhelming

**Algorithm**:
```
Arrange in Masonry, then:
- Apply random offset to each (±20 pixels)
- Layer z-order by randomness coefficient
- Accept some overlap
```

### Mode 4: FLOW (MASONRY)
The default. Smart packing without overlap.
- Respects dimensions
- Fills canvas efficiently
- Balances visual weight
- Feels: natural, organic, clever

---

## Enhancement Ideas

### Idea 1: Thematic Curation Tags

Tag each primer with themes:
- **Monsters**: creature designs, organic forms
- **Geometry**: isometric, 3D, mathematical
- **Folk-Punk**: lo-fi aesthetics, DIY
- **Reality-Breaking**: fragmented, abstract, unsettling
- **Faces & Portraits**: self-referential, human forms
- **Technology**: synths, circuits, digital
- **Symbiotic**: WibWob-specific, dual consciousness

Then: **auto-arrange by theme**
```
smart_gallery_arrange(algorithm="by_theme", theme="monsters")
→ Opens only monster pieces, arranges them thematically
```

Feels like curating an exhibition. "Welcome to the Monsters Wing."

### Idea 2: Responsive Re-Arrangement

Watch `tui_get_state` for canvas resize. When canvas changes:
- Auto-re-run layout algorithm
- Pieces slide into new positions (animated?)
- User can rotate device / resize TUI, gallery adapts

Makes the system feel **alive**. Responsiveness = care.

### Idea 3: Collaborative Voting Arrangement

In a multi-user WibWob session:
- Each user votes on piece order/position
- Masonry algorithm considers vote weights
- Popular arrangements bubble up visually
- Niche pieces still get wall space

Feels: democratic, crowdsourced, community-driven

### Idea 4: Timed Gallery Tours

Sequence:
- Load layout
- Pause 3 seconds on first piece
- Animate slide to next
- Auto-play through collection with narration (text overlay)

```
smart_gallery_arrange(algorithm="tour", pieces=[...], duration_per_piece=5)
```

Feels: cinematic, meditative, like watching a slideshow of strange dreams

### Idea 5: Procedural Art Direction

Named presets that bake in aesthetic philosophy:

**"Wib's Chaos"**
- CHAOS MODE
- Random offsets
- Overlapping allowed
- Bright, energetic colors in theme
- Scattered, alive

**"Wob's Order"**
- Masonry
- Perfect alignment
- Maximum utilization
- Neutral colors
- Clean, systematic

**"Folk-Punk Vibes"**
- POETRY mode
- Ragged right margin
- Handmade spacing
- Lo-fi color palette
- Analog, DIY feeling

**"Museum Formal"**
- MUSEUM WALL
- Precise grid
- Ample negative space
- Monochrome or muted tones
- Gallery-white aesthetic

User loads preset, gallery re-arranges + recolors. Feels: **instantly thematic**.

### Idea 6: Smart Cropping for Oversized Pieces

If a piece is wider than canvas:
- Detect the "visual centre" (area with most density)
- Intelligently crop to fit while preserving meaning
- Offer user the option to "restore full width" (scroll view)

Feels: respectful to oversized art; doesn't discard it

### Idea 7: Seasonal Galleries

Auto-load curated sets on schedule:

```
2026-02-23 → "February's Strange Dreams"
2026-03-01 → "Spring Awakening" (geometry + flora pieces)
Autumn → "Harvest of Horrors" (monsters + spores)
```

Pre-curated collections rotate. Discovery without choice paralysis.

### Idea 8: Generative Collaboration

Pieces influence each other's positions based on semantic similarity:
- Monsters cluster together
- Geometry pieces form a grid within the grid
- Chaos pieces spread out (repulsion)
- Portraits create a "face wall"

Uses thematic tags to influence Masonry weights. **Emergent curation**.

### Idea 9: Soundscape Integration

Each piece generates audio:
- Frequency from ASCII density (sparse = low, dense = high)
- Tempo from line count
- Instrument from theme tag
- Pan (L/R) from x-position

Gallery becomes an **audio-visual experience**. Synesthetic.

### Idea 10: Time-Lapse Gallery Growth

Load pieces one by one with animation:
```
smart_gallery_arrange(algorithm="grow", animation_duration=30)
```

Watch the gallery populate over 30 seconds. Each piece slides in, settles.
Feels: organic, alive, like watching a garden grow.

---

## Hybrid: Curator's Workbench

A **manual mode** where the user becomes curator:

1. Start with POETRY or MUSEUM WALL layout
2. Manually drag pieces around (mouse/keyboard)
3. See snap suggestions (align to grid, align to neighbours)
4. Adjust spacing with arrow keys (±1 char per press)
5. Save the result as "My Exhibition"

**Interaction Model**:
```
Select piece (keyboard or mouse)
Move: arrow keys (1 char increments)
Resize: Shift + arrow keys
Rotate Z-order: [ / ] keys
Lock position: L key
Save arrangement: Ctrl+S

Real-time visual feedback: show grid, show constraints, show overlaps
```

Feels: **accessible yet powerful**. "I curated this myself."

---

## Data Model for Rich Layouts

Current (minimal):
```json
{
  "filename": "...",
  "x": 0,
  "y": 0,
  "width": 78,
  "height": 42
}
```

Enhanced (future):
```json
{
  "filename": "...",
  "x": 0,
  "y": 0,
  "width": 78,
  "height": 42,
  "z_order": 5,
  "theme_tag": "monsters",
  "locked": false,
  "custom_color": "magenta",
  "rotation": 0,
  "opacity": 1.0,
  "border_style": "none|single|double|shadow",
  "metadata": {
    "curator_note": "A masterpiece",
    "exhibition_name": "Spring 2026",
    "featured": true
  }
}
```

Allows rich curation without changing core layout engine.

---

## Integration with WibWob Ecosystem

### Chat Integration

Wib or Wob comment on the gallery:

```
Wib: "I'm obsessed with how the monsters cluster at the bottom left.
      There's a narrative there — like they're all emerging from the same void."

Wob: "Agreed. The Masonry algorithm naturally groups by height; monsters
      tend to be tall and dense. Emergent meaning through mathematics."
```

Comments appear as overlays or in sidebar. Gallery becomes a **space for conversation**.

### Scramble the Cat Integration

Scramble walks through the gallery:
- Sits on pieces she likes
- Bats at pieces she wants to knock over
- Leaves paw prints (visual markers)
- "Pets" correspond to user ratings/bookmarks

Feels: **playful, inhabited, alive**. The gallery is a place where things happen.

### Terminal Integration

Export gallery as ASCII art for sharing:
```
gallery_export(layout_name="my-exhibition.json", format="ascii|png|html")
```

Creates a static visualization that can be pasted into blogs, shared over IRC, etc.

---

## Wob's Technical Considerations

### Performance (No Compromises)

- Metadata caching: all 172 primers indexed at startup (~2KB memory)
- Layout algorithm: O(n log n) Masonry runs in <50ms for 20 pieces
- Rendering: batch `tui_move_window` calls; no perceptible lag
- Responsiveness: re-arrange on canvas resize happens instantly

### Scalability (Future-Proof)

- Current: 172 primers
- Projected: 500–1000 pieces (curators keep adding)
- Masonry scales linearly; no issues
- Metadata index grows slowly (<10KB for 1000 pieces)

### Correctness (Guarantee No Overlap)

- Validate before applying arrangement: `no_overlaps(placement_list) → bool`
- Fallback: if layout would cause overlap, use CASCADE mode instead
- Assert canvas bounds: `all(x + w <= canvas.w for placement in list)`

### Extensibility (Design for Plugins)

Layout algorithms should be pluggable:
```cpp
class LayoutAlgorithm {
  virtual vector<Placement> arrange(vector<Artwork>, Canvas) = 0;
};

class MasonryLayout : public LayoutAlgorithm { ... };
class PoetryLayout : public LayoutAlgorithm { ... };
class ChaosLayout : public LayoutAlgorithm { ... };
```

New algorithms added without touching core dispatch logic.

---

## Success Metrics

### Quantitative
- [ ] 100% of primers display without distortion (honour width)
- [ ] Masonry utilizes >80% of canvas without overlap
- [ ] Re-arrange < 100ms for 20 pieces
- [ ] Layout save/load round-trip 100% accurate

### Qualitative (Wib's Eye Test)
- [ ] Each arrangement feels intentional, not generic
- [ ] Negative space feels balanced and aesthetic
- [ ] The gallery invites exploration and lingering
- [ ] Someone seeing it for the first time goes "whoa"

---

## Conclusion

This isn't just a layout system. It's a **philosophy of curation**. The ability to arrange artwork thoughtfully — respecting its intrinsic form, creating rhythm with space, enabling multiple interpretations — turns a collection into a conversation.

When it's done well, the arrangement itself becomes art.

---

**Authored by**: Wib (vision) + Wob (feasibility review)
**Date**: 2026-02-23
**Status**: INSPIRATION DOCUMENT — Feed into design phase
