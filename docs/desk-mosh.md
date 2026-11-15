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
| `mosh_way` | `way=wrap\|tear\|mosh\|bleed\|raw\|timelag\|all\|sound` | `all` (default) changes way every two bars through the six ways; `sound` lets the newest sound pick (see The clock) |
| `mosh_amount` | `value=0..100` | strength and share of the shard budget used (default 70) |
| `mosh_tempo` | `bpm=20..400`, or `0` | internal clock (default 128); `0` = steps only from the TR-808 or `mosh_step` |
| `mosh_seed` | `n=` | same seed and same steps give the same frames |
| `mosh_count` | `value=1..4000` | most shard windows at once (default 240) |
| `mosh_scope` | `scope=desktop`, or `scope=window id=wN` | one window moshes, the rest stay clean |
| `mosh_step` | `hits=kick,snare,tick,rat,metal,accent`, `n=` | runs steps now (tests, scores) |
| `mosh_screen` | `value=on\|off` | the optional screen layer (below) |
| `mosh_score` | `path=FILE`, `text=...`, or `path=off` | the mosh follows a score of parts (see A mosh score) |
| `mosh_hit` | `hits=kick,snare,tick,rat,metal,accent` | a hit played by hand now, as the keys and a MIDI keyboard do |
| `mosh_freeze` | `value=on\|off\|toggle` | the shards stay where they are and no step is taken |
| `mosh_record` | `value=on path=FILE [cast=FILE.cast]`, `value=off` | records a performance as steps (see Record and play back) |
| `mosh_play` | `path=FILE [fast=1]` | plays a recording back step for step |
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

## The ways

| way | in the film | on the desk |
|---|---|---|
| wrap | rows read at the wrong width; each kick a new width | band shards (3 rows, cut to the budget) read the desktop at width W + error; each kick picks the error, ticks slip the phase, ratchets add one or two; real windows with a grow handle are resized to the wrong width on kicks |
| tear | bands shear on kicks, rows repeat on snares, ratchets roll | band shards move sideways by a per-band amount that peaks on the kick; on a snare rows repeat in a stutter; ratchets roll the rows |
| mosh | old glyphs in new colours; blocks not redrawn drift | the film's datamosh: most blocks carry last step's shown cells along their own vector, so shapes smear into trails; refreshed blocks take the true desktop, half keeping old glyphs in the new colours; some blocks also drift as windows; on a crash blocks read at half pitch |
| bleed | colour changes dropped, palette rotates, glyphs decay to shade blocks | as the film: the palette is the desktop's own colours (most used first, at most 16, written back in the form the desktop used), 4-row bands move ink and paper along it on snares, colour changes are dropped so the last colours run on over the next glyphs (the glyphs stay legible), and 4x2 blocks decay to ░▒▓█ by paper brightness, inked in the paper colour on the next palette colour. Real windows keep their colours |
| raw | zoomed cells, stale rows, scrolling, bursts with no cursor moves | 2-row band shards: zoom x2/x3 on kicks, bands left stale, rows scroll up on snares, ratchet bursts read the desktop as one stream at the wrong width; a framed x2 zoomed copy of a real window |
| timelag | (the timelag film: slit-scan time displacement) | band shards whose cells read the desktop as it was: each cell lags f x L behind now, f set by the lag map of the bar (rows, six bands, rings, column pairs, single cells), L in seconds on the beat. Drum hits widen L (kick 0.5 s, snare 0.25 s, crash 1 s), a held synth note narrows it by a fifth a step, between them it drifts back to 1.5 s (at most 6 s) |

## The screen layer (optional)

`mosh_screen value=on` adds a per-cell filter over the desktop rows of the screen after the windows, for what
windows cannot show: colour runs and decay across window edges, rows read at the wrong width across the whole
desktop. `TWwdosApp::getEvent` calls `deskmosh::screenBefore()` before the base `getEvent` (which flushes
the screen to the terminal when it waits) and `deskmosh::screenAfter()` when it returns; `tick()` (from
`idle()`) restores first, so idle work, IPC and `look_study` see the true screen. Only cells that still hold
the value the filter wrote are put back, so a view that drew in between keeps its drawing. No change to the
vendor/tvision fork was needed.

## Timelag's memory

Timelag needs the past: every step pushes the step's material (the true desktop) into a ring of past frames,
whatever way is showing, so timelag has a past the moment it starts. The ring keeps at most 48 frames: 48 x the
desktop's cells x `sizeof(TScreenCell)` (24 bytes) is 8.8 MB on a 160x48 desktop. At 128 BPM 48 sixteenths are
5.6 s, so lags beyond that read the oldest frame. `mosh_status` reports `ring_frames`, `ring_bytes`, `lag` and
`lag_map`. Turning the mosh on empties the ring.

## Live sources

A terminal window (a film or a program playing in it, which `terminal_pause` and `terminal_resume` hold and
play) is a live source: when its cells change between steps the shards are cut again from it without moving the
beat (in wrap, tear, bleed and timelag; mosh and raw keep their own flow), at most every 60 ms. Every step
already cuts shards from every window's own cells, so any window that draws while the mosh runs is followed.

## Play it

While the mosh is on, these keys play it and are used up (not typed):

| key | does |
|---|---|
| Shift+F1 | kick (accented) |
| Shift+F2 | snare |
| Shift+F4 | hat |
| Shift+F5 | ratchet |
| Shift+F6 | crash |
| Shift+F7 | next way: wrap, tear, mosh, bleed, raw, timelag, all, sound |
| Shift+F8 | freeze, and again to release |
| Shift+F9, Shift+F10 | amount down, up by 10 |

Shift+F3 is left out: terminals send it as the same bytes as a cursor position report, and it does not arrive.
A hit played by hand is a step now. A MIDI keyboard (CoreMIDI in, `app/sound/midi`) plays it too while the mosh is
on: General MIDI drum notes are hits (35, 36 kick; 38, 39, 40 snare; 42, 44, 46 hat; 37 and the toms 41-50
ratchet; 49, 51, 52, 55, 57, 59 crash; velocity 110 and up accents), CC 1 (the mod wheel) sets the amount, CC 64
(the sustain pedal) freezes while held. Other notes go to the synth as before, and the mosh hears them through
the ears. With the mosh off every MIDI message goes to the synth.

## Record and play back

`mosh_record value=on path=FILE` starts a fresh mosh and records every step to a text file: the seed and settings,
then one line a step with its time on the sound clock, its position in the bar, its hits, whether a synth note
was held and the way it showed, and every way, amount and freeze change:

    seed 5
    way all
    amount 70
    count 240
    at 0.0 step -1 kick,accent held=0 way=wrap
    at 812.4 way timelag
    at 903.1 step -1 kick,metal held=0 way=timelag
    end 2419.7

`mosh_record value=off` saves it. `mosh_play path=FILE` starts a fresh mosh with the recording's settings and plays
the steps at their times (`fast=1` plays them all at once); the TR-808, the internal clock, the ears, the keys and
MIDI are ignored while it plays. On the same desktop the frames come out byte for byte.

`cast=FILE.cast` also writes the shown frames as an asciinema cast (asciicast v2): a header line with the screen's
width and height, then one `[seconds, "o", text]` event per step that changed the screen, carrying the rows that
changed with 24-bit or 16-colour SGR codes and UTF-8 text (Turbo Vision's code page 437 bytes are translated).
`asciinema play FILE.cast` plays it in any terminal.

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

    WWDOS_BIN=build-mosh/app/wwdos python3 tests/resident/test_desk_mosh.py        # 107 checks, 18 groups
    WWDOS_BIN=build-mosh/app/wwdos python3 tests/resident/desk_mosh_demo.py --out DIR [--keep]

Groups: timelag, live, keys, record, follow, bleed, hold, sound, residents, score, screen, ways, input, restore, seed, log, scope, bench. The demo starts an EGA desktop with an editor,
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
- Timelag's ring holds 48 steps; at slow tempi that is a long time but few moments, at fast ones a short time.
- The cast is the window layer only: the optional screen layer's filter is not in it, and a reframe of a live
  source between steps is a frame of its own.
- A recording replays exactly only on the same desktop; it records the steps, not the windows.
- Shift+F3 cannot be used (see Play it).
