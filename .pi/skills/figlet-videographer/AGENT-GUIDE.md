# Agent Guide: Figlet Typographic Videographer

Practical guidance for AI agents (Claude, pi, Codex) helping compose and record typographic poems in WibWob-DOS.

## Golden Rules

1. **Measure before placing** — always use `preview_figlet` with `info=true` to get exact window dimensions before writing x,y coordinates. Never guess sizes.
2. **Reading order matters** — layouts must read left→right, top→bottom like a book. The poem must be legible to a human scanning naturally.
3. **Fit the canvas** — sum line heights BEFORE writing JSON. If total > canvas height, shrink fonts or merge lines. The canvas is typically 152×46 in gallery mode.
4. **Creative voice/font choices** — don't default to the same voices and fonts every time. Consider the poem's subject, mood, pacing. A funeral poem wants different textures than a playground chant. Explore the full palette — `say -v '?'` lists all voices, `list_figlet_fonts` shows all 148 fonts. Match typographic weight to semantic weight. Match voice character to word meaning.
5. **Small primers only** — primers over 45w × 22h overwhelm the composition. Stick to the small ones (see list below). Place in sidebar at x≥120, never overlapping words.
6. **Clean canvas first** — `close_all` twice with 0.5s waits. The TUI needs time to redraw.

## Layout Workflow

### Step 1: Pick words and fonts, measure everything

```python
# For each word, get exact window size
info = post("preview_figlet", {"text": "grows", "font": "larry3d", "width": "0", "info": "true"})
# Returns: {"window_width": 47, "window_height": 12, "wrapped": false, ...}
```

### Step 2: Plan lines on paper

Each frame = one visual line. Stack lines top-to-bottom. Calculate:
- `line_end_x = 4 + sum(window_widths) + gaps` — must be ≤ 125 (leaving primer sidebar)
- `total_y = sum(max_heights_per_line)` — must be ≤ canvas height (46)

### Step 3: Check for wrapping

If `wrapped: true`, the word is too long for that font. Pick a smaller font or split the word. Common culprits:
- `isometric1` wraps words > 5 chars
- `banner3-D` wraps words > 8 chars
- `larry3d` handles most words up to ~10 chars

### Step 4: Write the JSON

Words with `x`,`y` spawn directly at position (no jump). Always include both.

### Step 5: Test with `--auto-layout` first

If you're unsure about manual positions, omit `x`,`y` from words and run:
```bash
python3 play.py timeline.json --auto-layout --mute
```
Auto-layout places words L→R, T→B with gap compression. Then screenshot and iterate.

## Font Sizing Reference

There are 148 fonts available (`list_figlet_fonts`). Always measure with `preview_figlet info=true` — don't memorise sizes. Key sizing facts:

- **window_height** = font_catalogue_height + 3 (chrome/borders)
- **window_width** = rendered text width + 6 (chrome/padding)
- Fonts range from 4-line (mini) to 11-line (isometric1) character heights
- Wider fonts (isometric1, banner3-D) wrap long words even at unlimited width — always check `wrapped` field
- **Choose fonts for the poem's mood**, not by rote. A delicate poem might use `small` everywhere. A brutalist piece might hammer `banner` and `block`. A dreamy one might float `larry3d` and `isometric1`. Let the subject matter guide typographic choices.

## Small Primers (safe to use)

These fit in the sidebar without overwhelming the poem:

```
20x 5  cave-monster.txt, cosmic-horror.txt, discophil.txt, rainbow-horror.txt, time-shaman.txt
27x 5  brain-loop.txt
20x 7  paradox.txt
20x 9  glum-face.txt, mini-herbivore.txt
20x11  iso-cube-cornered.txt
20x12  cat-cat-simple.txt, chaos.txt
20x13  filing-cabinet.txt
22x13  3dboy.txt
27x11  texture-grid.txt
```

Place at `x: 120-135, y: <same as current line>`. One primer per 2-3 frames max — don't clutter.

## Common Mistakes

### 1. Layout overflow
**Wrong**: 6 frames × doom font (11h each) = 66 rows. Canvas is 46.
**Fix**: Merge frames — put more words per line. Use smaller fonts for connective words.

### 2. Words covering primers
**Wrong**: Word at x=110, primer at x=108.
**Fix**: Words stay left of x=110. Primers go at x≥120.

### 3. Stale canvas on recording
**Wrong**: Previous composition visible in first frame of recording.
**Fix**: `record.sh` now clears before recording. If running manually, call `close_all` + wait 1s before play.

### 4. Speech cut off on last word
**Wrong**: Script exits immediately after last word, killing `say` process.
**Fix**: `play.py` calls `wait_for_speech(timeout=4)` after final frame.

### 5. GIF is fuzzy
**Wrong**: `agg --font-size 14` — too small for retina displays.
**Fix**: Use `--font-size 28` (2×). record.sh does this by default.

### 6. MP4 encoding fails with "height not divisible by 2"
**Wrong**: GIF has odd pixel dimensions.
**Fix**: ffmpeg `-vf "pad=ceil(iw/2)*2:ceil(ih/2)*2"`. record.sh does this.

### 7. `say` voices not in MP4
**Wrong**: `say` plays live but isn't captured.
**Fix**: record.sh pre-renders all speech to .aiff files and mixes them with backing track via ffmpeg `amix`.

## Audio Guide

### Backing tracks (from wibandwob-heartbeat)
```
DNB:     kibble-sommelier/audio/02-kibble-dnb.mp3 (45s)
Garage:  wake2-fluff-garage/fluff-garage.mp3 (45s)
Chiptune: blue-peter-chiptune.mp3 (28s)
Hiphop:  kibble-sommelier/audio/03-kibble-hiphop.mp3 (45s)
Italo:   kibble-sommelier/audio/04-kibble-italo.mp3 (45s)
Atmos:   spark-session-001/01-substrate-midnight.mp3 (50s)
```

### Stingers (2-3s, good for word reveals)
```
scramble-microfilm-v2/interstitials/recursive-stinger.mp3
scramble-microfilm-v2/interstitials/classified-stinger.mp3
cowork-oops/but-stinger.mp3
```

All paths relative to `/Users/james/Repos/wibandwob-heartbeat/output/`.

## Recording Checklist

1. TUI running in tmux session `wwdos`
2. API server on :8089
3. Run from a **real terminal** (not pi/codex) — asciinema needs TTY
4. Terminal window should be at least as wide as canvas (152+ cols)
5. `brew install asciinema agg` if not already installed
6. `record.sh timeline.json --gif --mp4`
7. Check output: `open examples/03-garden-poem-av.mp4`

## Iteration Pattern

When the human says "too floaty" or "can't read it":

1. Screenshot → identify overlaps and reading order breaks
2. Re-measure all words with `preview_figlet info=true`
3. Recalculate line totals (width ≤ 125 for words, height ≤ 46 total)
4. Tighten: merge small connective words ("through the" → one word), downsize fonts
5. Re-run `play.py --mute`, screenshot, confirm with human
6. Only then do the audio/recording pass
