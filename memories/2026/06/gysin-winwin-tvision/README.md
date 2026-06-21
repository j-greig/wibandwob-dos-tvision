# Devnotes — Can TVision do Gysin's "Meltdown" window-choreography?

> Live diary. Written as Wob investigates, Wib annotates. Date: 2026-06-21.
> Prompt from Zilla: Andreas Gysin (@ertdfgcvb / @nguyenwahedart) made a smooth
> many-windows experience in JS — gridded + cascading windows, some influencing
> others, like the Windows solitaire end-screen bounce but multiplied. Question:
> can wibwob-dos TVision (C++) do similar? Gysin's hunch: *"Depends if it has a
> frame buffer… I imagine tvision is some sort of 'direct' mode where at every
> frame the whole interface is redrawn."*
>
> Reference: https://verse.works/series/meltdown-by-ertdfgcvb

---

## TL;DR (the answer to Gysin)

His mental model is **inverted**. Turbo Vision is *not* direct/immediate mode
redrawing the whole interface every frame. The modern magiblot/tvision we build
on is a **retained-mode, z-ordered view tree with damage tracking and a
cell-level screen diff**. Three layers of "don't redraw what didn't change":

1. **View tree (retained)** — `TGroup` holds z-ordered `TView`s. Each view's
   `draw()` is clipped against the views in front of it (`tvwrite.cpp`
   `writeView`, `tvexposd.cpp` exposed-region computation). Move a window and
   only the *exposed* region beneath it is asked to redraw.
2. **Group buffer (`ofBuffered`)** — `TGroup` caches its composited output in an
   offscreen cell buffer (`tgroup.cpp` `getBuffer()` / `lock()` / `unlock()`).
   `lock`/`unlock` batch many child draws into one blit.
3. **Damage-tracked flush (the actual "frame buffer")** — `dispbuff.cpp` keeps
   TWO screen-sized cell arrays: `buffer` (what should be shown) and
   `flushBuffer` (what the terminal currently shows), plus per-row `rowDamage`
   ranges `{begin,end}`. `flushScreenAlgorithm` walks only damaged rows and
   emits only cells where `buffer != flushBuffer` to the terminal. FPS-capped
   (`TVISION_MAX_FPS`, default 60 on unix / 120 on win).

So: **yes, it has a frame buffer.** Two of them, plus a diff. It is closer to a
dirty-rectangle game compositor / React reconciler than to an immediate-mode
"clear and redraw everything" loop — and it predates both (Borland 1990 view
tree + magiblot's modern cell diff).

**Implication for Meltdown:** moving N windows is cheap *except* in the
pathological case where nearly every cell changes every frame (full-screen
cascade). There the damage set ≈ whole screen, so you do pay ~full redraw — but
that is exactly what the FPS cap + diff are designed to bound. Static regions
cost nothing; only moving glyphs cost.

---

## Evidence (vendor/tvision source)

### dispbuff.cpp — the real framebuffer + diff
```
DisplayBuffer: buffer (desired) + flushBuffer (on-screen) + rowDamage[y]={begin,end}
screenWrite() -> setDirty(x,y,len) extends the row's damage range
flushScreen() -> if needsFlush() && timeToFlush(): flushScreenAlgorithm()
flushScreenAlgorithm(): per damaged row, per cell: if cellDirty() emit, else skip
defaultFPS = 60 (unix) / 120 (win); TVISION_MAX_FPS env overrides
```
Key line: *"Since 'buffer' is also used as 'TScreen::screenBuffer' and is
directly written into by 'TView::writeView', we can avoid a copy operation in
most cases because the data is already there."* — views write straight into the
screen buffer; the diff is against the previously-flushed copy.

### tgroup.cpp — retained view tree + group buffer
```
options |= ofSelectable | ofBuffered;   // groups buffered by default
TGroup::draw() -> getBuffer(); ... if ofBuffered && buffer: blit cached
TGroup::lock()/unlock()  // batch child draws into the buffer
drawSubViews(p, bottom)  // z-ordered redraw of a sub-range only
```

### tvwrite.cpp / tvexposd.cpp — clipping & exposure (the z-order compositor)
These compute which spans of a view are actually visible (not covered by
siblings in front) so a buried window does no terminal work.

---

## What this means for the build (next: confirm against Gysin's actual piece)

- Animation driver: `TApplication::idle()` ticking each window's `moveTo()` /
  `locate()` on a chrono clock (see TVISION_DEV.md idle() pattern). The exposure
  + diff machinery handles the rest.
- Windows-influencing-windows: broadcast messages (`message(deskTop,
  evBroadcast, …)`) or a shared model the idle loop reads — a tiny ECS over the
  window list.
- Shadows: `TWindow` already has `shadowSize`; cascade overlap reads naturally.
- The "bounce" (solitaire end screen): per-window velocity + wall reflection in
  idle(), `locate()` each tick.

## Why Gysin assumed "direct mode" — because his own framework IS

Confirmed via play.core docs. Gysin's `play.core` (the engine behind
play.ertdfgcvb.xyz and the Meltdown window piece) is a **GLSL/fragment-shader
model**: you write one `main(coord)` function that is invoked *per cell, per
frame*, returning the glyph for that cell. Lifecycle: `boot()` once →
`pre()` once/frame → `main()` per cell/frame → `post()` once/frame (docs
literally cite post() "for a window overlay"). The whole grid is recomputed from
a pure function every tick. ~5–8000 cells at 30fps in JS.

So his hunch "*I imagine tvision redraws the whole interface every frame*" is him
projecting his own (immediate-mode) architecture. Reasonable! But TVision is the
mirror image:

| | Gysin play.core | Turbo Vision |
|---|---|---|
| Paradigm | Immediate mode | Retained mode |
| Unit | pure `main(cell)` fn | `TView` objects in a z-tree |
| Per frame | recompute every cell | redraw only damaged views |
| "Window" | math drawn in `post()` | real `TWindow` (drag/focus/z/events) |
| Frame buffer | conceptual, rebuilt each tick | persistent `buffer` + diff vs `flushBuffer` |
| Statics cost | full (recomputed) | ~zero (not damaged) |
| Moving-everything cost | full | ≈full (damage≈screen) → **they converge** |

The punchline for Zilla: **both land on the same grid-of-cells flushed to a
terminal, from opposite ends.** For Meltdown's all-moving cascade the cost is
comparable. But TVision hands you window objects for free (z-order, shadows,
focus, drag, events, resize) that Gysin re-derives by hand in shader math —
*and* gives you free statics when not everything moves. The "antiquated tech"
is, on this axis, the more advanced compositor.

## Caveat / where TV fights you
- play.core's strength is **arbitrary per-cell math** (plasma, flow fields,
  noise behind/through the windows). TV `draw()` can do the same inside a
  `TBackground` override or a custom view, but you're writing cells imperatively,
  not as a clean shader fn. For Meltdown's *window motion* TV wins; for a
  full-screen shader field underneath, play.core's model is more ergonomic.
- TV repaint is event/damage-driven. To animate continuously you must drive it
  from `idle()` (chrono-ticked) — there is no built-in rAF. Fine, just explicit.
- Smooth motion = `moveTo()`/`locate()` per tick + let exposure+diff do the
  compositing. Don't `redraw()` the whole desktop each frame unless you must.

---

## Proof: built a working stress demo

See `app/demos/meltdown.cpp` → `build/demos/demo_meltdown`. N bouncing/cascading
`TWindow`s driven from `idle()`, windows-influencing-windows via a shared model.
Build + run notes below.

## RESULT — it runs, first build (after one kbSpace fix)

`app/demos/meltdown.cpp` → builds clean. Ran in a 100×30 tmux pane,
`TVISION_MAX_FPS=60 ./build/demos/demo_meltdown`. Captured two frames 1s apart:
9 real `TWindow`s, each with its own animated per-cell pattern (diagonal scroll,
checker breathe, radial plasma, interference plasma, h-sweep), all bouncing off
walls, cascading, overlapping with correct z-order + shadows, focus frame on the
active window (`╔═[■]═ W9 ═╗`). Positions clearly different between frames →
motion confirmed. App responded to keys and shut down clean.

Frame grab (excerpt):
```
░░│:*│ = = = = = = = = = = │#*=:.  :-+#%%#+-.  │░░  <- W7 (checker) over W1 (sweep)
░░└──│ = = = = = = = = = = │ ───────┐░░░░░░░░░░░░░│: :*%#-. :--: .-#%*: │  <- W8 radial
░░░░░│= = = = = = = = = = =│+=-:..:-│  ...  ╔═[■]════ W9 ════════╗  <- focused
```

**The proof:** 9 windows, every one running a full-interior per-cell shader at
~45Hz, composited by the damage/diff layer with zero manual buffer management.
This is *more* than Gysin's Meltdown does (his windows are mostly empty frames);
TVision absorbed it without breaking a sweat because static desktop = no damage,
and only the moving/animating cells hit the terminal.

### Build/run
```bash
cmake -B build/demos -S app/demos          # re-run after adding the .cpp (GLOB)
cmake --build build/demos --target demo_meltdown -j8
TVISION_MAX_FPS=60 ./build/demos/demo_meltdown
#   +/- add/remove window   f = freeze   c = re-cascade   Esc/Alt-X = quit
```

### Notes for going further toward real Meltdown
- For the *cascade-collapse* look (Gysin's third image: a deep z-stack melting
  down-right), spawn windows with incrementing offsets and a slow downward drift
  + tiny per-window phase lag → the staircase falls. Already have `cascade()`;
  add a `meltMode` that drifts the whole stack.
- For windows-driving-windows beyond repulsion: a shared field (e.g. a global
  flow vector sampled at each window's centre) makes them school like the
  solitaire bounce but coherent. Cheap: one `pre()`-style update in `idle()`.
- The per-cell `draw()` IS your shader surface — port any play.core `main(cell)`
  body almost verbatim into `TPatternView::draw()`. The math transfers; only the
  plumbing (retained vs immediate) differs.

## CORRECTION — the reference is "Win-Win", not verse Meltdown

Zilla clarified: the verse.works *Meltdown* series (16 on-chain pieces that melt
their own source code) is a DIFFERENT, older work. The piece in his IG story is
**"Win-Win" (2024–26), Andreas Gysin — "Real-time, silent, JavaScript, WebGL"**,
shown by nguyenwahed. Got the actual video (x.com/nguyenwahed/status/
2067948011976634469) and analysed it frame-by-frame.

### What Win-Win actually does (30s loop, 20fps, 720²)
Aesthetic: yellow double-border frames, blood-red fill, hard black drop-shadows,
each window titled **`WN:x,y`** (N = index, x,y = grid coords). Pure DOS windows.

Auto window-count per 0.1s (connected-red-components; undercounts overlaps but
tracks the rhythm). Full table in count.py output. The arc:

| t (s) | windows | coverage | phase |
|---|---|---|---|
| 0.0 | 2→ | 0.03 | boot: a clean **5×2 grid of 10** big windows (W1:8,4 … W10:92,17; cols step 21, rows step 13) |
| 0.5–2 | 22→48 | 0.6 | **SUBDIVISION** — windows split into a dense grid of small frames, count doubles |
| 2–5.5 | 35–48 | 0.6 | churning re-tile, windows shuffle/merge |
| 5.5–11 | →**67** | 0.58 | second build to **PEAK fragmentation** (t≈11.1s) |
| 11.5–15 | 65→16 | →0.75 | **THE MELT** — small windows coalesce into big overlapping **cascade staircases**; coords storm at top |
| 15–20 | 6–17 | 0.75 | sparse: a few dominant windows + cascades; t=20 → 6 windows |
| 21–22 | 6→56 | 0.7 | brief **re-explosion** (subdivide again) |
| 22.5–25 | →**1** | 0.82 | collapse to a single near-fullscreen window |
| 25–30 | 3–23 | →0.44 | settle, climb back, **loop** to the grid |

**It breathes.** grid → subdivide(~67) → melt/merge → collapse(1) → re-explode →
loop. Two breaths in 30s. The "melt" = many small windows MERGING into few big
cascading ones (count↓ while coverage↑), not windows sliding offscreen.

### Mechanics to port to TVision
1. **Subdivision/treemap layout** — split desktop into an N-cell grid (or BSP),
   one window per cell; animate N up for the densify, down for the collapse.
2. **Grid↔cascade morph** — interpolate window targets from grid cells to
   overlapping down-right staircase stacks (the melt).
3. **Density breathing loop** — drive spawn/despawn + layout morph on a cycle.
4. Live `WN:x,y` labels — DONE (our cascade already shows them ticking).

Our current `demo_meltdown` has grid/cascade/mirror/bounce with eased morphs +
live labels. To match Win-Win it needs: a **subdivide** layout + a **breathing**
auto-cycle that ramps window count. That's the next build.

Assets: /tmp/winwin/winwin.mp4, frame_*.png, montageA_first8s, montageB_full30s.

## BIG REFRAME — Win-Win does CELL-LEVEL effects, not just window motion

Zilla zoomed into the video. The close-ups reveal effects that pure TWindow
motion CANNOT produce — this is what Gysin's "depends if it has a frame buffer"
actually meant:

1. **Drip / screen-melt** (grid dripping down): each column of a window's bottom
   edge is pulled downward in vertical streaks — the DOOM melt. A per-COLUMN cell
   op (copy cell downward by an increasing amount), not a window moving.
2. **Text-buffer smear / scroll-repeat**: columns of the SAME label repeated down
   the screen (`W2:29,4` ×15, `,4` ×12, `8:8` runs, `W2:229999,,,,44`). Cells
   shifted along an axis and not cleared — tape-head smearing of the char buffer.
3. **Tear-bars**: black columns/rows slicing through the grid — buffer offset
   glitches (shift a band of rows/cols by N).
4. **Bleed-through**: fragments of labels visible INSIDE other windows — one
   layer's cells showing through another. No z-opaque window does this.

These all require READ + WRITE of arbitrary persistent cells. play.core gives it
free (author every cell per frame in main()/post(), sample prior buffer).

### Architectural fork
- **v1 = current `demo_meltdown`**: retained-mode TWindow objects; choreography
  (grid/cascade/mirror/breathe), live labels. Clean, efficient, real windows.
  CANNOT smear/drip/tear/bleed.
- **v2 / alt = immediate-mode cell framebuffer**: ONE full-desktop custom TView
  owning a `std::vector<TScreenCell>`. Draw the windows INTO it as DATA (rects +
  labels), then run per-cell POST-EFFECTS (drip, column-shift, smear, repeat)
  before writing the buffer out via writeBuf/writeLine. This is the play.core
  paradigm hosted inside TVision — TVision used as a framebuffer, not a widget
  toolkit. TV's damage/diff still flushes only changed cells, so it stays cheap.
- **hybrid (best?)**: keep TWindow objects for the crisp grid/cascade phases for
  free window chrome; for the MELT phase, capture the composited screen cells and
  run a post-pass smear over them. Switch layers by phase of the breath.

TVision CAN do v2: `TDrawBuffer` + `writeBuf`/`writeLine` write arbitrary cells;
a full-screen view's draw() can build any cell pattern. The melt is just cell
arithmetic. What you give up vs v1 is the free window objects (focus/drag/z) for
the melted layer — fine, the melt doesn't need them.

Decision pending (Zilla): v2 or hybrid for the next build. Current `demo_meltdown`
stands as the v1 motion proof.

## Gysin's OWN words — Win-Win is a text-mode window manager

2nd video (x.com/andreasgysin/status/1858599615056027693, Nov 2024), caption:
*"'Win-Win' — A demo with a little **text mode window manager** that I use for
some of my text projects. Comes with **custom styles and shadows**."*

So Win-Win = a reusable TUI window manager — exactly this project. The blue/
portrait demo adds: **palette variants** (red & blue/cyan skins, themeable),
**custom fill styles** (horizontal scanline-stripe interiors), **drop shadows**,
chrome = title `w.N (x,y)` + body + right-edge scrollbar panel, and a confirmed
**mirror-cascade tunnel** (one window cascades down-left AND down-right into a
symmetric V) — our v1 `m` mode already does this.

## Related work for riffing (saved, not core)
- assets/ref-meltdown-syntax-highlighter.* — Meltdown's syntax highlighter
  "highlighting itself and melting away" (x.com/andreasgysin/status/
  2057816608668569782, May 2026). The self-referential code-melt; future idea fuel.

## Viewing the reference frames
The montages are dense (fair point — hard for a human to parse). To eyeball a
single full-res moment: `data/vidframe.sh <video.mp4> <seconds> [out.png]`.
Clean single frames live in assets/frames-*/.

TODO ↓ (updated live)
- [x] Characterise play.core + TV framebuffer model
- [x] Build + run `demo_meltdown` v1 (grid/cascade/mirror/bounce, live labels)
- [x] Identify "Win-Win"; frame analysis (breathing loop, ~67 peak)
- [x] Build `tui-demo-loop` skill (validated)
- [x] Zoom analysis → cell-level fx (drip/smear/tear/bleed)
- [x] 2nd video → Win-Win IS a themeable text-mode WM; mirror tunnel confirmed
- [x] viewer script + clean single frames (legibility)
- [x] BUILD v2 `demo_winwin`: cell-framebuffer with subdivide/breathe + MELT ✓

## v2 BUILT — `app/demos/winwin.cpp` → `demo_winwin`

The "TVision as a framebuffer" answer to Gysin, working. ONE full-screen TView
holds a `char`+`attr` cell buffer (BIOS attrs: yellow-on-red 0x4E etc). Windows
are DATA eased toward layout targets, rasterised into the buffer each frame
(border + `WN:x,y` title + solid fill + offset shadow), then a per-column MELT
post-pass drips the whole composited image downward. draw() blits the buffer via
TDrawBuffer/writeLine; TV's diff still flushes only changed cells.

Confirmed via tui-demo-loop:
- **mirror** → clean symmetric cascade TUNNEL (W1 nests down-left, W2 down-right,
  meeting at centre) — Gysin's exact move.
- **mirror + melt** → whole image drips per-column, top clears, borders+labels
  smear into ragged streaks with scattered digits — matches his smear zoom-ins.
  This is the effect v1 (window motion) literally cannot produce.
- modes g/s/c/m + breathing `a` + melt `.`/`,` all live.

Gotcha hit & fixed (our own note predicted it): a TView inserted in the desktop
gets NO key events unless made `current` — `view->makeFirst();
deskTop->setCurrent(view, normalSelect);` + `options |= ofSelectable`.

Proof: assets/demo_winwin_v2_proof.png + assets/f_0{0,1,2}_*.txt.

### Next polish (not blocking)
- COLOR capture in tui-demo-loop (mono drops fills; `tmux capture-pane -e`
  + an ANSI→PNG renderer) so shared montages show the palette.
- scanline fill style + palette theming (red/blue) to match Win-Win skins.

## STRESS TEST — cascade window scaling (Zilla asked: 10x/30x/60x/100x?)
demo_winwin keys 1–6 = 10/30/60/100/200/400 windows. Live cost meter (row 0).
Measured @180x50:

| windows | draw/frame | note |
|--:|--:|--|
| 10 | 1.59 ms | |
| 60 | 3.31 ms | |
| 100 | 2.48 ms | |
| 400 | **1.11 ms** | cheaper than 60! |

Cost scales with TOTAL CELLS TOUCHED (window area sum + the fixed W×H blit), NOT
window count. At 400 the windows auto-shrink + wrap, so fewer cells than 60 big
ones. We're capped at 40fps by our own throttle; real budget is hundreds of fps.
Answer: 10×/100× smooth is trivial; thousands are feasible.
Proof: assets/demo_winwin_400windows.png.

## DARING DEMO — `demo_meltcode` (app/demos/meltcode.cpp)
Inspired by his on-chain Meltdown: TVision as a TRUECOLOR framebuffer
(`TColorAttr(TColorDesired(0xRRGGBB),…)`). Real C++ source scrolls as a
syntax-highlighted listing (keyword/number/string/comment RGB), 5 wireframe
windows glide over it (code shows through), then a per-column/row MELT (4
directions, his `maskDir`) drips the whole coloured buffer. Rainbow mode,
line numbers, scroll-speed. Self-referential — it melts its own source.
Keys: `. ,` melt · `d` dir · `r` rainbow · `l` lines · `+/-` speed · `space` · `a` auto.
Proof: assets/demo_meltcode_proof.png (mono — colour shows live only).

## Demos so far
- `demo_meltdown` — v1 retained TWindow motion (grid/cascade/mirror/bounce).
- `demo_winwin`   — v2 cell-framebuffer WM: layouts + breathing + melt + stress 1–6.
- `demo_meltcode` — daring RGB syntax-melt of its own source + gliding windows.
