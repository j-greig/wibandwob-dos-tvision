# Gysin's Meltdown source — code style & how it maps to ours

Source: https://meltd.ooo/standalone/index.html?tokenId=N  (N=0..15).
One 59KB self-contained ES module (`<script id="meltdown" type="module">`).
All 16 editions are the SAME file — `tokenId` only selects a palette. Saved:
`meltdown_token0_MONO.{html,module.js}` (+ token4, token9 html).

NB: this is the **Meltdown** code (a syntax highlighter that renders its OWN
source into a melting grid). It is the sibling of **Win-Win** (the window
manager). Same author, same engine family — both are a cell-grid + per-column
melt. Mine it for ENGINE STYLE, not for window-manager specifics.

## His architecture (and our equivalent)

| Gysin (JS, immediate-mode) | Ours (`winwin.cpp`, TVision) |
|---|---|
| `class Char {x,y,idx,fgColor[3],bgColor[3]}` — one struct per cell, RGB fg/bg, `Object.preventExtensions` for speed | `char ch[] + uchar at[]` parallel arrays; attr is BIOS byte (no RGB) |
| `class Grid2 {width,height,data}` typed-array framebuffer with `fillRect/fill(fn)/loop(fn)/set/get` | our `std::vector<char>/<uchar>` + `put()`; `compose()` = his fill loop |
| `SFC32 extends ARandom` — seeded PRNG (deterministic per token) | `rand()` / `hashx()` per-column; could swap in SFC32 for determinism |
| `TokenTypes {DEFAULT,KEYWORD,IDENTIFIER,NUMBER,STRING,COMMENT,OPERATOR,PUNCTUATION}` + per-palette color tables | we use 1 flat theme (yellow-on-red); could add token coloring for a code skin |
| `maskDir / maskOffset / maskHeight` — the MELT is a per-column mask offset | our `meltPass()` per-column downward offset — SAME technique, confirmed |
| `scrollOffsetX/Y, scrollDirX/Y, scrollTimeout` — content scrolls in viewports | not yet; windows are static-content. Add scroll for code-in-window |
| `dragActive, dragStart*, dragStartScroll*` — mouse drag to pan | TVision gives drag free via TWindow (v1); v2 would handle evMouse |
| `messageTimeout, promptStr, rainbowMode, lineNumbers, highlightTokens` | UI/feature flags; ours: melt/auto/freeze toggles |

## Takeaways to guide our build
1. **Cell = explicit struct with fg+bg** is the right primitive. To match his
   colour range we'd move from BIOS attr bytes to RGB (TVision supports TColorRGB
   in TColorAttr) — needed for the syntax-palette skins.
2. **Melt = per-column mask offset** — we already do this. His `maskDir` means the
   melt can run in any of 4 directions (up/down/left/right), not just down. Easy
   extension to `meltPass()`.
3. **Seeded PRNG (SFC32)** gives deterministic, reproducible pieces (important for
   on-chain). If we ever mint/loop deterministically, port SFC32 instead of rand.
4. **Content scrolling inside windows** (`scrollOffset`) is the missing verb for a
   "code melting in windows" variant — a viewport offset into a text buffer.
5. His whole thing is ONE immediate-mode grid redraw per frame — exactly our v2
   `demo_winwin`. Confirms TVision-as-framebuffer is a faithful port path.

## Upstream engine: play.core
https://github.com/ertdfgcvb/play.core — Gysin's open ASCII playground/engine.
The `main(coord, context, cursor, buffer)` per-cell-per-frame model (boot→pre→
main→post) is the immediate-mode paradigm all the Meltdown/Win-Win pieces sit on.
(We have some play.core repo material elsewhere; not far in yet.) Port reference
for: per-cell shader fns, the buffer/cursor abstractions, font-metric handling.
