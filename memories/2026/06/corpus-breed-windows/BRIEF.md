# BRIEF — CORPUS: winning artworks that breed & mutate in windows

> Nascent. Enhanced as research proceeds. Date 2026-06-21.
> Ask (Zilla): take the golden-corpus winners (heartbeat /output ASCII pieces he
> loves), display them in TVision windows, and test ways to BREED and MUTATE the
> artworks in and between windows. STARK monochrome (no colour). Build on the
> effects that already work in demo_winwin. Ship a TVision demo.

## Why this is on-brand (from the corpus evidence)
golden-corpus/winners.yaml has Zilla's verbatim notes on 39 winners. The signal:
- He loves **dense modular wholes** — "a whole made of modules", "living machine
  of related parts", "modules… wonder-ful in the literal way".
- He loves **hand-drawn feel** over code-grid (counterexamples are "obviously
  made with code, lifeless, boring layout").
- KEY: *"primers have been rebred"* and the file's own FYI: **"'primers rebred'
  is OFTEN the implicit meaning of 'wibwobby'/'vibey'/'weird'."**
  → Breeding IS his core aesthetic. This demo makes that literal & live.

## Source material (winners with ASCII .txt, art in <art>…</art> blocks)
(heartbeat output/, line counts of the .txt)
- catnip-sequencer-02-purrr-oscillations-iii-v1  (70L)  — "VERY good" (deep study)
- cloud-scallop-variations-02-mares-tails-v1     (60L)  — "well 'drawn'… novel"
- thinkism-04-umwelt-triptych-v1                 (71L)  — triptych = 3 panels
- ribosome…06-golem-the-unmade-v1                (112L) — figure
- digital-shamans-the-descent-line-v1            (100L) — "primers rebred"
- outsider-art-for-llms-02-ledger-of-marks-v1    (99L)  — "infopacked"
- before-the-split-the-prophetic-section-v3b     (119L) — "modules → whole"
- who-watches-03-eye-of-eyes-v1                   (54L)  — eyes (window-wall vibe)
- the-lingering-cloud-01-ballast-v4              (120L) — "dense and packed"
(who-watches-02-monitor-wall is PNG-only, no txt — but the concept = wall of
 windowed monitors, which this demo basically is.)

## Demo concept — `demo_corpus` ("the breeding tank")
A stark mono window manager whose windows hold winner-artwork SPECIMENS, which
mutate and crossbreed. Reuses the demo_winwin cell-framebuffer engine
(immediate-mode, per-column melt, eased motion), stripped to monochrome.

Genetics (the new verbs):
1. **specimen** = a char grid (one winner's <art>, ASCII-ised, mono).
2. **MUTATE(s, rate)** — per-cell ops drawn from a small set, inspired by
   beastie-cat-mutants + the melt we already have:
   - density-drift: nudge a glyph up/down a ramp " .:-=+*#%@" (grow / erode)
   - smear: copy a neighbour cell (the melt family)
   - decay: blank a cell (rot)
   - speck: inject a mark into empty space (accretion)
3. **BREED(A,B,mask)** → child specimen (a new window labelled "A×B"). Masks:
   vertical-half · horizontal-half · column-stripe · checker · quadrant ·
   value-noise threshold. Crossover = pick each child cell from A or B by mask.
4. **SEAM-BREEDING (between windows)** — where two windows OVERLAP on screen, the
   compositor blends the overlap (front XOR/alternate back) instead of opaque
   cover → hybrids shimmer in the seams, live, with no explicit breed command.

Modes/keys (planned): cascade/grid display · `m` mutate focused · `b` breed two ·
`x` toggle seam-breeding · `.`/`,` melt · arrows/`tab` select · space freeze · auto.

Aesthetic: white/grey on black, hard shadows, thin frames, `W:slug` labels. NO
colour (Zilla: stark, monochrome — the wibwobdos vibe). Density over prettiness.

## Open questions (resolve as I read the art)
- [ ] art dimensions of each piece (need similar-ish sizes to breed cleanly?)
- [ ] best 4–6 starting specimens (distinct, stark, dense)
- [ ] crossover on different-sized grids: pad to max, or breed on overlap only?
- [ ] runtime-load (like demo_golem ART_PATH) vs embed — runtime, multi-path arg.

## Decisions (from reading the art)
- Format varies: some pieces wrap art in `<art>…</art>`, some are tagless raw
  ASCII (catnip = a fake geocities cat-band page, pure ASCII). LOADER: extract
  `<art>` if present, else take the whole file. asciiize() (from golem) collapses
  UTF-8 box/shade glyphs to single-cell ASCII — fixes width AND rendering.
- Pieces are ~50–90 wide, 50–130 tall (display). Too tall for one screen, and
  breeding needs uniform grids → SAMPLE each artwork to a fixed SPECIMEN canvas
  (nearest-neighbour scale-to-fit, e.g. 46×22) so the whole piece reads AND all
  specimens are the same size → crossover is trivial.
- Starting specimens (distinct, stark, dense): catnip, cloud-scallop, thinkism,
  digital-shamans, who-watches eyes, outsider-ledger.
- Each WINDOW OWNS a specimen (so bred children carry their own grid).

## v1 → v2: "contained/static/dead" → ALIVE (Zilla feedback)
v1 was a tidy grid that did nothing until a keypress — not wibwobby. The corpus
says wibwobby = primers rebred, dense, churning, alive. v2 makes the tank
METABOLIZE by default:
- **L_SOUP layout** — packed, heavily overlapping scatter (not a grid) → dense
  interwoven mass; seam-breeding fires everywhere.
- **ambient mutation** — every frame lightly mutates CHILDREN (originals kept as
  pristine gene-stock) → constant shimmer/life.
- **auto-breed churn** — random cross-pollination every ~0.9s; population grows
  to a cap (14) then culls the oldest CHILD → ever-changing gene pool.
- **breathing melt wave** — a gentle traveling drip always on, ebbing near zero
  so it reads at troughs.
- **two-octave swim** — windows wander, never settle.
Result: left alone it goes 6 → 14 specimens of hybrids (ledgxeyes, thinxcatn,
clouxth…), a churning overlapping field that's never the same frame to frame.

## Status — SHIPPED
- [x] read golden-corpus REPORT + winners.yaml; breeding = the thesis
- [x] found winners with .txt; tagged vs tagless loader
- [x] specimen-canvas sampling + loader
- [x] build demo_corpus (load 6 winners, grid/cascade/soup, mutate, breed, seam, melt)
- [x] v2 aliveness pass (soup + ambient mutate + churn + melt wave + swim)
- [x] proof captured: assets/demo_corpus_proof.png (v1), demo_corpus_alive.png (v2)
- demo: app/demos/corpus.cpp -> build/demos/demo_corpus
- keys: Tab focus · m mutate · b breed(focus×next) · x seam · c/g/(default soup) · . melt · a auto · Space freeze

## Future
- weight breeding toward OVERLAPPING pairs (true cross-pollination of neighbours)
- per-specimen "metabolism" (density breathing) so even un-bred winners pulse
- arg to point at any corpus slug list; pull winners.yaml directly
- a "lineage" label trail (catnip→catnxclou→…) so you can read the genealogy
