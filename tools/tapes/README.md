# Film tapes

Short spans of Wib&Wob's terminal films stored as the films' own cells, played as text in a WIBWOB-DOS
terminal window while the talk player plays the matching mp3.

| path | what |
|---|---|
| `modules/wibwob-2026/tapes/INDEX.json` | the tapes: `{"tapes":[{slug, title, what, film, from, to, duration, cols, rows, fps, file, audio}]}` |
| `modules/wibwob-2026/tapes/<slug>.tape.gz` | the frames (format below) |
| `modules/wibwob-2026/tapes/<slug>.mp3` | the film's shipped mix for exactly `from`..`to`, 44.1 kHz stereo; frame 0 = audio 0 |
| `tools/tapes/tape_play.py` | the player (Python 3 standard library; runs on macOS `/usr/bin/python3` 3.9) |

`title` is a world word for the window title. `what` is one plain sentence about what is on screen, for the
director model. `film` is the source film folder in wibandwob-heartbeat `output/`; `from`/`to` are seconds in
that film.

## Play a tape

```bash
python3 tools/tapes/tape_play.py modules/wibwob-2026/tapes/all-six.tape.gz --at $(python3 -c 'import time; print(time.time() + 2)')
```

- The terminal needs at least `cols` x `rows` cells (80x50 for most tapes, so a window of 82x52 with its
  frame; `walk-edge` is 45x28).
- `--at EPOCH` is wall-clock time (`time.time()`) for frame 0. Start the mp3 at the same EPOCH and the two stay
  in sync: every frame is scheduled from EPOCH, late frames are dropped, nothing drifts.
- `--loop` repeats the tape. `--stats FILE` writes `{shown, dropped, late_max, ...}` as JSON for timing tests.
- It prints only the frames: cursor hidden, screen cleared, cursor shown and colours reset at the end, exit 0
  (also on SIGINT, SIGTERM and SIGHUP).

## Tape format

gzip text. Line 1 is a JSON header `{"v":1,"cols":80,"rows":50,"fps":12,"frames":N,"duration":S,"title":"..."}`.
Each further line is one whole frame: `ESC[H`, then for every row `ESC[<r>;1H` followed by exactly `cols` cells
written as truecolor SGR runs (`38;2;r;g;b`, `48;2;r;g;b`), then `ESC[0m`. No newline inside a frame.

The films draw every glyph in one cell (their font squeezes wide glyphs to 8 px). A glyph that a terminal would
draw two cells wide is replaced by a one-cell stand-in that looks like the film's glyph (`つ` → `ɔ`, `︻` → `▄`,
`︿` → `∩`, `꒰` `꒱` → `(` `)`, `ヽ` → `\`), so every row is exactly `cols` cells in any terminal. Glyphs the
WIBWOB-DOS terminal cannot show (it prints U+FFFD for `ꞈ` `ᴥ` `ᴗ` `ᵕ` and gives `ᆽ` zero width) are replaced too:
Scramble's mouth `ꞈ` → `ﻌ` (the house's typed mouth), `ᴥ` → `ω`, `ᴗ` `ᵕ` → `◡`, `ᆽ` → `⊼`. The tapes contain
no two-cell glyphs; if one ever appears it is written as the glyph followed by nothing for its second cell.

## Timing (measured)

At 12 fps every tape played with 0 dropped frames in tmux at 192x54 and inside a WIBWOB-DOS terminal window
(82x52 at 192x54), on-screen frame within one frame of schedule. At 15 fps WIBWOB-DOS dropped up to 15% of
frames on some tapes, so the tapes are 12 fps. The player writes only the rows that changed since the frame on
screen, which is what keeps the busiest tapes (3D scenes, about 38 KB a frame) inside the terminal's budget.

## Running it in a terminal window

Run the player as the window's program (`exec python3 tools/tapes/tape_play.py ...`) so no shell prompt is
drawn over the last frame when it ends. It clears the window at start, so a typed command line does not show.

## Make or change tapes

The baker lives in wibandwob-heartbeat: `scripts/termfilm/tape_bake.py` (the tape list is at its end).

```bash
cd ~/Repos/wibandwob-heartbeat
uv run scripts/termfilm/tape_bake.py ls                  # the tapes
uv run scripts/termfilm/tape_bake.py bake [slug ...]      # write tapes, mp3s and INDEX.json here
uv run scripts/termfilm/tape_bake.py verify [slug ...]    # every tape against its shipped mp4, cell by cell
uv run scripts/termfilm/tape_bake.py check <slug> --at S  # side-by-side PNG: shipped mp4 | tape frame
```

The baker reads the cells each film composes before it rasterises (never its pixels), decodes every baked frame
again with a terminal model and compares it with the film's own raster at the same time.
