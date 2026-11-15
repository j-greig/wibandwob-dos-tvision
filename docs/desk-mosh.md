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
| `mosh_way` | `way=wrap|tear|mosh|bleed|raw|all` | `all` (default) changes way every two bars |
| `mosh_amount` | `value=0..100` | strength and share of the shard budget used (default 70) |
| `mosh_tempo` | `bpm=20..400`, or `0` | internal clock (default 128); `0` = steps only from the TR-808 or `mosh_step` |
| `mosh_seed` | `n=` | same seed and same steps give the same frames |
| `mosh_count` | `value=1..4000` | most shard windows at once (default 240) |
| `mosh_scope` | `scope=desktop`, or `scope=window id=wN` | one window moshes, the rest stay clean |
| `mosh_step` | `hits=kick,snare,tick,rat,metal,accent`, `n=` | runs steps now (tests, scores) |
| `mosh_screen` | `value=on|off` | the optional screen layer (below) |
| `mosh_status` | | settings, shard count, clock source, step and loop times |
| `mosh` | `args="way tear"` | the same commands as words, for the chat: `on`, `off`, `panic`, `way X`, `amount N`, `tempo N`, `seed N`, `count N`, `scope window wN`, `step HITS`, `screen on` |

## How it works

The window manager does the damage. On each step the mosh opens, moves, refills, restacks and closes real
Turbo Vision windows ("shards", type `mosh_shard`). Each shard holds a copy of desktop cells: one glyph, one
ink, one paper per cell, on the grid.

- **The material** is the true desktop: the screen as it was when the mosh started, with every real window's
  own buffer laid on it back to front. Shards are never part of it, so the mosh never feeds on itself except
  where a way asks for that (mosh reads its own last frame).
- **The real windows stay underneath.** Shards are not selectable, so the focused window keeps the focus and
  the person's typing lands in it. A left click on a frameless shard goes through to the real window under it
  (the first click selects that window, as on any Turbo Vision desktop; the next one drags it). A right-drag
  grabs a shard: it leaves the mosh and stays where it is put until the mosh is off. A framed shard (the raw
  zoom copy) drags and closes by its frame like a window.
- **The caret stays true**: a shard that would cover the focused view's caret steps off the desk for that step.
- **Off** closes every shard (held ones too), puts each real window back to the size it had if the mosh
  resized it and nobody moved it since, puts back window colours the mosh changed if nobody changed them since,
  and redraws.
- A menu or dialog that is open pauses the mosh.

## The clock

Steps come from the TR-808 window when it plays (its `tr808_step` events, queued and run from `idle()`, never
from inside the drum machine's timer), else from the internal tempo in sixteenths, else (tempo 0) only from
`mosh_step`. Voices map to the film's hits: bd = kick; sd, cp = snare; ch, oh = tick; rs, cb, cl, lt, mt, ht =
ratchet; cy, ma = metal. The internal pattern has kicks with pushes, claps on 4 and 12, hats on the offbeats,
a ratchet at the end of each bar and a crash every fourth bar. Nothing is random per frame: every choice is a
hash of the seed and the step counters, so a test can compare frames.

## The five ways

| way | in the film | on the desk |
|---|---|---|
| wrap | rows read at the wrong width; each kick a new width | band shards (3 rows, cut to the budget) read the desktop at width W + error; each kick picks the error, ticks slip the phase, ratchets add one or two; real windows with a grow handle are resized to the wrong width on kicks |
| tear | bands shear on kicks, rows repeat on snares, ratchets roll | band shards move sideways by a per-band amount that peaks on the kick; on a snare rows repeat in a stutter; ratchets roll the rows |
| mosh | old glyphs in new colours; blocks not redrawn drift | the film's datamosh: most blocks carry last step's shown cells along their own vector, so shapes smear into trails; refreshed blocks take the true desktop, half keeping old glyphs in the new colours; some blocks also drift as windows; on a crash blocks read at half pitch |
| bleed | colour changes dropped, palette rotates, glyphs decay to shade blocks | band shards with colours rotated in 4-row bands on snares, colour runs (a cell keeps the previous cell's colours), decay to ░▒▓█ by paper brightness (at least ░, so dark papers keep a texture); real windows' colour families rotate on snares |
| raw | zoomed cells, stale rows, scrolling, bursts with no cursor moves | 2-row band shards: zoom x2/x3 on kicks, bands left stale, rows scroll up on snares, ratchet bursts read the desktop as one stream at the wrong width; a framed x2 zoomed copy of a real window |

## The screen layer (optional)

`mosh_screen value=on` adds a per-cell filter over the desktop rows of the screen after the windows, for what
windows cannot show: colour runs and decay across window edges, rows read at the wrong width across the whole
desktop. `TWwdosApp::getEvent` calls `deskmosh::screenBefore()` before the base `getEvent` (which flushes
the screen to the terminal when it waits) and `deskmosh::screenAfter()` when it returns; `tick()` (from
`idle()`) restores first, so idle work, IPC and `look_study` see the true screen. Only cells that still hold
the value the filter wrote are put back, so a view that drew in between keeps its drawing. No change to the
vendor/tvision fork was needed.

## Residents and the log

- `acts` records who turned it on and off (`mosh on (way all, amount 70, seed 1)`, `mosh off after N steps
  (on by X)`); shards add no act each.
- `look_glance` lists real windows only and adds `"mosh":{...}`; `look_glance shards=1` lists every shard
  (type `mosh_shard`, owner `mosh`). `describe` answers for a shard.
- `look_changes` leaves shards out and adds one summary: steps, shards opened and closed since that reader's
  last look.
- `look_study` reads the true screen (with the screen layer on too) and says a mosh is showing.

## Tests and demonstration

    WWDOS_BIN=build-mosh/app/wwdos python3 tests/resident/test_desk_mosh.py        # 60 checks, 8 groups
    WWDOS_BIN=build-mosh/app/wwdos python3 tests/resident/desk_mosh_demo.py --out DIR [--keep]

Groups: screen, ways, input, restore, seed, log, scope, bench. The demo starts an EGA desktop with an editor,
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

- The material behind a window that later moves is the screen from when the mosh began, so a moved window
  leaves its old picture there in the material.
- The film's moshes cut between different stills, so "old glyphs in new colours" always shows; a desktop that
  barely changes shows it only where something moves (the TR-808 playhead, typing).
- Bleed often reads as flat slabs: decay comes in 4x2 clusters and rotated EGA blues turn into large single
  colour areas. The film's wide-glyph overflow is not ported; it depends on the terminal mis-wrapping, which a
  cell-true desktop does not do.
- Raw's scrolling and bursts are drawn into shards; the film let the real terminal scroll and wrap.
- The TR-808 publishes only steps with hits, so its silent steps do not move the mosh.
- A left click cannot drag a frameless shard (it goes through to the desktop); only a right-drag grabs one.
- A shard over the caret leaves for the step, which leaves a hole in the picture around the caret.
- `get_state` lists every shard; the window id map is rescanned per lookup, which is slow with hundreds of
  shards (look_glance hides them by default).
- At 800 shards a step costs about 50 ms.
