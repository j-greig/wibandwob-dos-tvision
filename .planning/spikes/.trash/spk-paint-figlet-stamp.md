# Spike: FIGlet Text Stamp in Paint

Status: done
GitHub issue: —
PR: —
Branch: `spike/paint-figlet-stamp`

## Goal

Add a one-shot "Stamp FIGlet Text" action inside the paint window.
User enters text, picks a font, and the rendered FIGlet output is stamped
onto the canvas as text-layer cells at the current cursor position.

## Design

- **UX**: `Edit > Stamp FIGlet...` menu item (not a persistent tool)
- **Rendering**: call existing `figlet::renderLines(text, font, cols)`
- **Stamping**: write non-space chars as `textChar` cells — transparent spaces
  so FIGlet doesn't bulldoze underlying pixel art
- **State**: `lastFigletFont_` + `lastFigletText_` on `TPaintWindow` for repeat use
- **Font picker**: reuse `showFontListDialog()` pattern from figlet_text_view
  (stretch: catalogue popup menu)

## Implementation

### P0 — Stamp with dialog + API (~200 lines)

- [x] `cmPaintStampFiglet = 630` command constant
- [x] `Edit > Stamp FIGlet...` menu item in paint window menu bar
- [x] `TFigletStampDialog`: text input + font TListBox + live TFigletPreview
- [x] `doStampFiglet()` uses dialog, transparent stamp (skip spaces)
- [x] IPC `paint_stamp_figlet` (id,text,font,x,y,fg,bg)
- [x] IPC `list_figlet_fonts` — JSON array of 148 fonts
- [x] IPC `preview_figlet` — render text+font to plain text (no canvas needed)

### P1 — Font picker (stretch)

- [ ] Catalogue popup menu (extract from `TFigletTextView::showFontMenu`)
- [ ] Preview in dialog before stamping

## Acceptance Criteria

- AC-1: User can stamp FIGlet text onto paint canvas via menu
  - Test: IPC `paint_stamp_figlet` + `paint_export` verifies text cells placed
- AC-2: Spaces in FIGlet output are transparent (don't overwrite pixel art)
  - Test: draw pixels, stamp over them, export shows pixels preserved where FIGlet has spaces
- AC-3: Stamp respects cursor position
  - Test: IPC set cursor, stamp, export verifies offset
- AC-4: Round-trip through save/load preserves stamped text
  - Test: stamp → save → clear → load → export matches
