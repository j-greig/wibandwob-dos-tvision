# Desk mosh

The desk mosh breaks the live desktop the way the catmosh film breaks its stills, in time with a beat,
while the real windows stay open and usable underneath. Turning it off puts the screen back byte for byte.

Code: `app/desk_mosh.cpp`, `app/desk_mosh.h`. Tests: `tests/resident/test_desk_mosh.py`. Demonstration:
`tests/resident/desk_mosh_demo.py`. Source film: `~/Repos/wibandwob-heartbeat/output/catmosh-2026-11-15/`
(`player_main.js` holds the five ways this module ports).

## Turn it on

- View > Desk Mosh on/off, or Alt-M.
- Esc twice within 0.8 s stops it, as does `mosh_panic`.
- From the socket, the chat, Scramble or a resident: the registry commands below (through `exec_command name=`).

| command | arguments | does |
|---|---|---|
| `mosh_on` | `way=` optional | starts the window layer |
| `mosh_off` | | closes every shard, puts back what it changed on real windows |
| `mosh_panic` | | the same, and stops the screen layer |
| `mosh_way` | `way=wrap\|tear\|mosh\|bleed\|raw\|all\|sound` | `all` (default) changes way every two bars; `sound` lets the newest sound pick (see The clock) |
| `mosh_amount` | `value=0..100` | strength and share of the shard budget used (default 70) |
| `mosh_tempo` | `bpm=20..400`, or `0` | internal clock (default 128); `0` = steps only from the TR-808 or `mosh_step` |
| `mosh_seed` | `n=` | same seed and same steps give the same frames |
| `mosh_count` | `value=1..4000` | most shard windows at once (default 240) |
| `mosh_scope` | `scope=desktop`, or `scope=window id=wN` | one window moshes, the rest stay clean |
| `mosh_step` | `hits=kick,snare,tick,rat,metal,accent`, `n=` | runs steps now (tests, scores) |
| `mosh_screen` | `value=on\|off` | the optional screen layer (below) |
| `mosh_score` | `path=FILE`, `text=...`, or `path=off` | the mosh follows a score of parts (see A mosh score) |
| `mosh_status` | | settings, shard count, clock source, step and loop times |
| `mosh` | `args="way tear"` | the same commands as words, for the chat: `on`, `off`, `panic`, `way X`, `amount N`, `tempo N`, `seed N`, `count N`, `scope window wN`, `step HITS`, `screen on`, `score PATH` |

## How it works

The window manager does the damage. On each step the mosh opens, moves, refills, restacks and closes real
Turbo Vision windows ("shards", type `mosh_shard`). Each shard holds a copy of desktop cells: one glyph, one
ink, one paper per cell, on the grid.

- **The material** is the true desktop: a capture of the desktop drawn with every shard hidden, with every
  real window's own buffer laid on it back to front each step. When a real window moves, opens or closes, the
  capture is taken again (the shards are hidden, the desktop is drawn and read, the shards are shown and
  repainted, all before the screen reaches the terminal), so shards take the new picture and nothing is left
  where a window used to be. Shards are never part of it, so the mosh never feeds on itself except where a way
  asks for that (mosh reads its own last frame). A scoped mosh whose window closes ends, and the log says so.
- **The real windows stay underneath.** Shards are not selectable, so the focused window keeps the focus and
  the person's typing lands in it. A left click on a frameless shard, or a press that moves at once, goes
  through to the real window under it (the first click selects that window, as on any Turbo Vision desktop;
  the next one drags it). A left press held still for 0.3 s, or a right press, grabs the shard: it leaves the
  mosh, follows the pointer and stays where it is put until the mosh is off. Let go while it moves faster than
  50 cells a second and it is thrown: it slides on, bounces off the desk's edges and slows to a stop. A
  resident's hand does the same with `resident_mouse action=drag button=right`. A framed shard (the raw zoom
  copy) drags and closes by its frame like a window.
- **The caret stays true**: a frameless shard over the focused view's caret (the terminal's, or the cell a
  view draws its own caret in, as the text editor does) is cut into up to four shards around it, so only the
  caret's own cell shows the real window.
- **Off** closes every shard (held ones too), puts each real window back to the size it had if the mosh
  resized it and nobody moved it since, puts back window colours the mosh changed if nobody changed them since,
  and redraws.
- A menu or dialog that is open pauses the mosh.

## The clock

Steps come from the TR-808 window when it plays, every step, silent ones too (`tr808_step` with hits,
`tr808_tick` for a silent step; the tick goes to the app only, never to socket subscribers), else from the
internal tempo in sixteenths, else (tempo 0) only from `mosh_step`. Steps are queued and run from `idle()`
when the sound clock (`sound::clockMs`, the one clock of the mixer that plays the synth and the TR-808)
reaches the time the step is heard, so the picture moves with the sound and not ahead of it by the stream's
lead. Synth notes, and drum rows that do not come from a playing TR-808 (the tracker), are read from the
ears (`hear`) on the same clock: every note-on tears (a kick's impulse; its pitch picks the bands), and a bass
note (below C-3) also picks the wrap width. With `mosh_way way=sound` the newest sound picks the way: a bass
note wraps, other notes tear, drums mosh, a second of quiet bleeds. Voices map to the film's hits: bd = kick; sd, cp = snare; ch, oh = tick; rs, cb, cl, lt, mt, ht =
ratchet; cy, ma = metal. The internal pattern has kicks with pushes, claps on 4 and 12, hats on the offbeats,
a ratchet at the end of each bar and a crash every fourth bar. Nothing is random per frame: every choice is a
hash of the seed and the step counters, so a test can compare frames.

## The five ways

| way | in the film | on the desk |
|---|---|---|
| wrap | rows read at the wrong width; each kick a new width | band shards (3 rows, cut to the budget) read the desktop at width W + error; each kick picks the error, ticks slip the phase, ratchets add one or two; real windows with a grow handle are resized to the wrong width on kicks |
| tear | bands shear on kicks, rows repeat on snares, ratchets roll | band shards move sideways by a per-band amount that peaks on the kick; on a snare rows repeat in a stutter; ratchets roll the rows |
| mosh | old glyphs in new colours; blocks not redrawn drift | the film's datamosh: most blocks carry last step's shown cells along their own vector, so shapes smear into trails; refreshed blocks take the true desktop, half keeping old glyphs in the new colours; some blocks also drift as windows; on a crash blocks read at half pitch |
| bleed | colour changes dropped, palette rotates, glyphs decay to shade blocks | as the film: the palette is the desktop's own colours (most used first, at most 16, written back in the form the desktop used), 4-row bands move ink and paper along it on snares, colour changes are dropped so the last colours run on over the next glyphs (the glyphs stay legible), and 4x2 blocks decay to ░▒▓█ by paper brightness, inked in the paper colour on the next palette colour. Real windows keep their colours |
| raw | zoomed cells, stale rows, scrolling, bursts with no cursor moves | 2-row band shards: zoom x2/x3 on kicks, bands left stale, rows scroll up on snares, ratchet bursts read the desktop as one stream at the wrong width; a framed x2 zoomed copy of a real window |

## The screen layer (optional)

`mosh_screen value=on` adds a per-cell filter over the desktop rows of the screen after the windows, for what
windows cannot show: colour runs and decay across window edges, rows read at the wrong width across the whole
desktop. `TWwdosApp::getEvent` calls `deskmosh::screenBefore()` before the base `getEvent` (which flushes
the screen to the terminal when it waits) and `deskmosh::screenAfter()` when it returns; `tick()` (from
`idle()`) restores first, so idle work, IPC and `look_study` see the true screen. Only cells that still hold
the value the filter wrote are put back, so a view that drew in between keeps its drawing. No change to the
vendor/tvision fork was needed.

## A mosh score

A text file of parts the mosh follows bar by bar, as the film follows its five parts:

    # comment
    bars 5  wrap  amount 70  tempo 138
    bars 9  tear  amount 80  tempo 200
    end            # or: loop

`bars N WAY` with optional `amount A` and `tempo BPM` (the tempo drives the internal clock; a playing TR-808
keeps its own, and tempo 0 stays manual). `end` turns the mosh off after the last bar; `loop` starts again.
`mosh_score path=scores/catmosh.mosh` loads the worked example: the film's five parts in its order, bars and
tempi (wrap 5, tear 9, mosh 7, bleed 12, raw 7 bars, taken from the film's score). A bad line is refused with
its number.

## Residents and the log

- Scramble's paws allow `mosh`, `mosh_on`, `mosh_off`, `mosh_panic`, `mosh_way`, `mosh_amount`, `mosh_score`
  and `mosh_status`; her prompt names `mosh args="..."`. Her desktop list leaves the shards out and adds one
  line saying the desk is moshing. The chat's tool (`tui_menu_command`) reaches every mosh command by name,
  and `tui_list_commands` lists them.

- `acts` records who turned it on and off (`mosh on (way all, amount 70, seed 1)`, `mosh off after N steps
  (on by X)`), a score loaded, and each grab or throw of a shard with who did it; shards add no act each.
- `look_glance` lists real windows only and adds `"mosh":{...}`; `look_glance shards=1` lists every shard
  (type `mosh_shard`, owner `mosh`). `describe` answers for a shard.
- `look_changes` leaves shards out and adds one summary: steps, shards opened and closed since that reader's
  last look.
- `look_study` reads the true screen (with the screen layer on too) and says a mosh is showing.

## Tests and demonstration

    WWDOS_BIN=build-mosh/app/wwdos python3 tests/resident/test_desk_mosh.py        # 84 checks, 14 groups
    WWDOS_BIN=build-mosh/app/wwdos python3 tests/resident/desk_mosh_demo.py --out DIR [--keep]

Groups: follow, bleed, hold, sound, residents, score, screen, ways, input, restore, seed, log, scope, bench. The demo starts an EGA desktop with an editor,
a primer and the TR-808 playing, moshes through the five ways for 20 seconds while it types into the editor,
and writes one `tmux capture-pane -e` snapshot per frame (5 a second). `--keep` leaves it running:
`tmux -L moshdemo attach`.

## Shard counts

Measured on an M-series Mac under load (tear way, amount 100, 128 BPM, 160x50):

| count | shards open | step | event loop |
|---|---|---|---|
| 100 | 100 | 1 ms | 25 ms |
| 300 | 270 | 3 ms | 26 ms |
| 1000 | 800 | 50 ms | 50 ms |

At 800 shards a step still lands inside the 117 ms sixteenth, but typing feels the 50 ms. The default is 240.
The band geometry (rows per band, two-cell minimum width) caps the count below the setting.

## What is weak

- The film's moshes cut between different stills, so "old glyphs in new colours" always shows; a desktop that
  barely changes shows it only where something moves (the TR-808 playhead, typing).
- Bleed's rotated bands are whole 4-row stripes across the desk; on a desktop with one main colour (the blue
  EGA texture) a band reads as one flat stripe, as some of the film's bands do. The film's wide-glyph overflow
  is not ported; it depends on the terminal mis-wrapping, which a cell-true desktop does not do.
- Raw's scrolling and bursts are drawn into shards; the film let the real terminal scroll and wrap.
- Retaking the true capture repaints the whole desktop twice in one step; a window dragged across the desk
  during a mosh costs that on every step it moves.
- A thrown shard bounces only off the desk's edges, not off windows.
- `get_state` lists every shard; the window id map is rescanned per lookup, which is slow with hundreds of
  shards (look_glance hides them by default).
- At 800 shards a step costs about 40 ms.
