# The machine's sound

The desktop has a sound of its own and its residents have ears. Three parts, all in `app/sound/`:

- **Synth** (`synth.cpp`, `synth_view.cpp`): a PC speaker square voice and a two-operator FM voice in the
  manner of the AdLib card, played from a window with the keyboard, by command, and by a resident.
- **Ears** (`ears.cpp`): what is sounding now and what sounded in the last N seconds, for the synth and the
  TR-808, as text from the engines' own state.
- **Event scores** (`score.cpp`, `score_view.cpp`): a short score that a person and a resident perform
  together, run by the app's clock.

The sound commands are registry commands: send them as `cmd:exec_command name=<name> ...` on the IPC socket, or
`POST /menu/command {"command":"<name>","args":{...}}` on the API server. Every command is logged in the acts
log with its author (`actor=`, `source=`), as resident senses describe (`docs/resident-senses.md`).

## Synth

### Voices

| voice | id | what it is |
|---|---|---|
| PC speaker | `pcspk` | One square wave, on or off, no volume. The pitch is the one the PC's timer chip makes: 1193182 Hz divided by a whole number, so A-4 is 439.97 Hz (divisor 2712). One voice: a new speaker note stops the one before. |
| FM | `fm` | Two operators (a modulator and a carrier) with modulator feedback and an ADSR envelope, nine channels as on the OPL2; a tenth note at once takes the channel that started first. Patches: `bass`, `organ`, `bell`, `brass`, `lead`, `pluck`, `chip`. |

Notes are written in tracker form: `C-4` is middle C (MIDI 60), `C#4`, `Db4`, or a MIDI number. A `.` in a
phrase is a rest.

### The window

Menu: View > Synth (PC Speaker, FM), or `exec_command name=open_synth`. Keys, in the tracker layout:

| keys | what |
|---|---|
| `z s x d c v g b h n j m ,` | C to C of the window's octave |
| `q 2 w 3 e r 5 t 6 y 7 u i` | C to C an octave up |
| Tab | PC speaker or FM |
| `[` `]` | octave down, up |
| `-` `=` | previous, next FM patch |
| `/` | gate length: 60, 125, 250, 500, 1000 ms |
| `0` | sound on or off |

A terminal sends no key releases, so every note lasts the gate length. The window shows the keyboard with the
sounding keys lit, the last notes with who played each, and its voice, patch, octave and gate.
`look_glance` gives one sentence: `synth, FM voice with patch bass, octave 4; 6 notes played, the last G-4 by wib
0.4 s ago; sound on`.

### Commands

    exec_command name=open_synth [voice=pcspk|fm] [patch=bass] [octave=4] [gate=250] [audio=0|1] [x y w h]
    -> {"ok":true,"id":"w3"}

    exec_command name=synth action=note note=C-4 [vel=100] [dur=250] [voice=] [patch=] [id=]
    exec_command name=synth action=notes notes="C-3 G-3 C-4 . G-3" step=125 [dur=] [vel=] [voice=] [patch=]
    exec_command name=synth action=voice value=pcspk
    exec_command name=synth action=patch value=bell
    exec_command name=synth action=octave value=3
    exec_command name=synth action=gate value=500
    exec_command name=synth action=audio on=0
    exec_command name=synth action=state
    exec_command name=synth action=render notes="A-4" voice=pcspk dur=1000 ms=1000 [from_ms= to_ms=]

`note`, `notes` and `open` open a window when there is none; the others need one. Every action but `render`
answers with the window's state:

    {"id":"w3","ok":true,"voice":"fm","patch":"bass","octave":4,"gate_ms":250,"audio":true,
     "audio_available":true,"played":3,"sent":3,"silenced":0,"patches":["bass",...],
     "recent":[{"note":"G-4","midi":67,"voice":"fm","patch":"bass","vel":100,"dur_ms":112,"ago_ms":40,"who":"wib"},...]}

`render` plays nothing. It renders the notes to a buffer and answers with numbers about it, the same for the same
notes every time:

    {"ok":true,"rate":22050,"samples":22050,"ms":1000,"fnv":"1587c6fa70f41e7c","peak":9010,"rms":0.275,
     "up_crossings":439,"notes":1}

### Sound out: one stream, one clock

Every sound joins one mixer (`app/sound/mixer.cpp`) at a sample time, and one stream (`app/sound/stream.cpp`)
plays the mixer without a break: an AudioQueue of 3 buffers of 256 samples at 22050 Hz mono. The mixer keeps the
voices across calls: one PC speaker voice (a speaker note from any phrase replaces the one sounding) and nine FM
channels (given out when a note starts; a tenth takes the oldest). The TR-808's recordings are mixed in on the same
stream. The offline `render` is the same mixer rendered in one block, and the mixer gives the same samples in any
block size.

The stream's samples are the machine's clock (`sound::clockMs()`): the TR-808's sequencer, the event scores and
the ears all take their time from it. A sound asked for at clock time T is placed at T plus a fixed lead of 1024
samples (46.4 ms), so it is never late and never waits for a timer: a key pressed in the synth window sounds 46 ms
later, every time. A TR-808 step lands on the sample it is due, not when the window's timer wakes. A score's
`do tr808 play` starts the drums on the section's bar line, so the score's bars and the drum steps share one origin
on one clock and cannot drift apart.

| `WWDOS_SOUND_OUT` | what |
|---|---|
| `coreaudio` (default on macOS) | the stream plays on the Mac's audio out; if the AudioQueue cannot open, it runs `null` and `sound action=state` says why |
| `null` | the stream runs at real time and plays nowhere (tests): the clock, mixing and capture all work |
| `off` | no stream: the clock is steady time, the synth plays one WAV per phrase through `afplay` and the TR-808 its samples through `afplay`, as before |

`WWDOS_SYNTH_AUDIO=0` and the TR-808's `WWDOS_TR808_AUDIO=0` still keep each instrument out of the mix; the ears hear
it either way, at the same times.

    exec_command name=sound action=state
    -> {"ok":true,"out":"coreaudio","running":true,"rate":22050,"block":256,"buffers":3,"lead_ms":46.44,
        "clock_ms":2483,"samples":55296,"notes":3,"hits":0,"late":0,"fm_busy":0,"speaker":false,"ringing":0,"note":""}

    exec_command name=sound action=capture ms=1200
    -> {"ok":true,"standing":"data","out":"coreaudio","rate":22050,"samples":26460,"from_ms":1330.98,"to_ms":2530.98,
        "peak":2134,"rms":0.0154,"onsets_ms":[1709.66,1830.2,1949.21]}

`capture` reads the last ms of the mix (the stream keeps 10 s) and finds onsets (a 64-sample frame whose level
jumps above four times the frames before), as times on the clock: the same times the ears give in `at_ms`.
Neither changes anything or is logged.

### A started drum machine is heard

A new TR-808 opens with an empty pattern. `play` (the command, the window's key, a score's `do tr808 play`) on a
pattern with no step and no accent on loads `classic-house` first, and the state says so in `start_preset`. A
pattern with any step on plays as it is. The drum machine's sound is on unless `WWDOS_TR808_AUDIO=0`.

## Tracker

Music written in rows, in the form of the 1987 trackers: channels across, rows down, each cell a note, an
instrument and an effect (`app/sound/tracker.cpp`, `tracker_view.cpp`). The worked example is
`tracks/arp-and-bass.trk`.

    00 C-2 01 G08 | C-4 06 A37 | --- .. ... | BD- 7F ...

| field | FM channel | speaker channel | drums channel |
|---|---|---|---|
| note | `C-4`, `C#4`; `===` note off; `---` empty | the same | `BD-` `SD-` `LT-` `MT-` `HT-` `RS-` `CB-` `CP-` `MA-` `CL-` `CY-` `OH-` `CH-` |
| instrument (hex) | patch 01-07: bass organ bell brass lead pluck chip; `..` the channel's | ignored | level 00-7F (`..` 0x66) |
| effect | `Axy` arpeggio (the row in three: the note, +x, +y semitones); `Vxx` velocity; `Gxx` gate in sixteenths of a row; `Fxx` speed or tempo | `Gxx`, `Fxx` | `Vxx` level, `Fxx` |

With no `G`, a note rings until the next note or note off in its channel's pattern (at most 16 rows). `Fxx` works
as in ProTracker: `F01`-`F1F` set the speed, the ticks of a 24-tick beat in a row (`F06` is 4 rows a beat, `F03`
8), and `F20`-`FF` set the tempo in beats per minute; both hold from their row until the next change.

### Patterns, the order, blocks, mute and solo

A song is a list of patterns, each with its own length (1-128 rows, up to 64 patterns), and an order list of
pattern numbers (up to 256). Play goes down the order; at its end it loops back or stops (`loop`).

    exec_command name=tracker action=new_pattern [rows=8 | copy=0]      # adds one and edits it
    exec_command name=tracker action=pattern value=1                     # edit pattern 1
    exec_command name=tracker action=pattern_rows pat=1 value=32
    exec_command name=tracker action=order value="0 1 0 2"
    exec_command name=tracker action=play [pos=2] [row=0]                # the song from an order position
    exec_command name=tracker action=play_pattern pat=1 [row=]           # one pattern, looped
    exec_command name=tracker action=copy pat=0 row=0 rows=4 ch=0 chs=2  # a block to the clipboard
    exec_command name=tracker action=paste pat=1 row=8 ch=0
    exec_command name=tracker action=mute ch=3 [on=0|1]  |  solo ch=3 [on=0|1]

`set`, `write`, `clear`, `copy`, `paste` and `cursor` take `pat=` (default: the pattern being edited). A column
copied from a drum channel pastes only onto drum channels, a note column only onto FM and speaker channels; cells
that do not fit are left as they were. The clipboard is shared by every tracker window. A muted channel, or every
channel but the soloed ones, plays no sound, sends no MIDI and is not heard by the ears.

Keys: `<` `>` previous and next pattern, Ctrl+K a new pattern, Ctrl+C copies the channel from the cursor down,
Ctrl+V pastes it at the cursor, Ctrl+E mutes and Ctrl+T solos the channel under the cursor (Ctrl+B, N, O and S
belong to the app's menus), Space plays the song, Enter plays the pattern being edited from the cursor row. Rows play at
`tempo` and `rpb` (rows per beat; 4 makes a row a sixteenth) on the sound stream, at the row's time on the one
clock. FM channels share the mixer's nine FM channels with the synth window; the speaker channel shares its one voice.

The window (View or `open_tracker`): arrows move (Left and Right step through note, instrument and effect), Tab moves
a channel, PgUp/PgDn a beat. In the note field the tracker keyboard writes a note (`z`..`m`, `q`..`i`; on the
drums channel the same keys pick BD, SD, LT ... in order) and moves down a row; `1` writes a note off; `.`,
Backspace or Delete clears. In the instrument field hex digits shift in; in the effect field `a`, `v` or `g`
then hex digits. `[` `]` octave, Space plays from the top or stops, Enter plays from the cursor's row, Ctrl+L
loops.

    exec_command name=open_tracker [track=arp-and-bass | text=<song> | channels="fm:bass fm:lead speaker drums" rows=16 tempo=120] [play=1]
    exec_command name=tracker action=set row=4 ch=1 cell="E-4 06 A37"
    exec_command name=tracker action=write ch=3 row=0 notes="BD- . SD- ." step=2 instr=7F    # down a column
    exec_command name=tracker action=cursor row=4 ch=1 field=note|instr|fx octave=4          # then resident_key types
    exec_command name=tracker action=play [row=]  |  stop  |  loop on=0|1  |  tempo value=  |  rows value=
    exec_command name=tracker action=clear row= ch=   |  clear all=1
    exec_command name=tracker action=save track=<name>    # writes tracks/<name>.trk
    exec_command name=tracker action=load track=<name> | text=<song>
    exec_command name=tracker action=text | state

The song file is plain text: the header, the order, then each pattern and its rows. A file with rows and no
`pattern` line (the first format, as `tracks/arp-and-bass.trk`) loads as pattern 0 in an order of 0.

    # wwdos tracker
    title arp and bass
    tempo 120
    rpb 4
    channels fm:bass fm:pluck speaker drums
    order 0 1 0
    pattern 0
    00 C-2 01 G08 | C-4 06 A37 | --- .. ... | BD- 7F ...
    ...
    pattern 1
    00 G-2 01 ... | --- .. ... | --- .. ... | BD- 7F F78
    ...

Track names are letters, digits, `-` and `_`; files live in `tracks/` (`$WWDOS_REPO/tracks`, `./tracks`, the
source tree). `state` gives `lines` (every row as above), `play_row`, `started_ms` and `row_ms`; `describe` gives
a sentence and the rows as text.

**The ears show the same rows.** `hear window=<tracker id>` adds `tracker_rows`: each row that sounded, in the
order it sounded (across patterns and tempo changes), printed as the tracker prints it, from the cells the ears heard. A cell that made no sound (a
note off) is printed `--- .. ...` there.

## MIDI

MIDI in and out through CoreMIDI (`app/sound/midi.cpp`), timestamped on the one clock.

- **In.** Every MIDI source (a keyboard plugged in later too) and a virtual destination named `WibWob-DOS` feed
  the front synth window (one opens if none is). A note on starts a held note at the key's velocity; the note off
  (or a note on with velocity 0) lets it go, so the gate is as long as the key was down and the release follows.
  The keys are the person's: each note on is an act by `person` (`midi note on C-4 v100 ch1 from keyboard`), and
  the ears hear the note held until it is let go (`dur_ms` is then the time it was held).
- **Out.** A virtual source named `WibWob-DOS` sends what the TR-808 and the tracker play: start (`FA`), clock
  (`F8`, 24 a beat), stop (`FC`), the TR-808's hits as General MIDI drums on channel 10 (bd 36, sd 38, lt 41, mt
  45, ht 48, rs 37, cb 56, cp 39, ma 70, cl 75, cy 49, oh 46, ch 42) with a note off 50 ms later, and the tracker's
  notes on channel n+1 for its channel n (drums on 10) with the note off one gate later. Every message leaves at
  its time on the clock plus the stream's lead, so it lines up with the audio.

| `WWDOS_MIDI` | what |
|---|---|
| `coremidi` (default on macOS) | the device above |
| `null` | no device: `midi action=in` stands in for a keyboard, and what would leave is logged (tests) |
| `off` | nothing in or out |

    exec_command name=midi action=state          # device, sources connected, counts, clock_out
    exec_command name=midi action=log n=100      # the last messages, in and out: ms, dir, from, hex, text
    exec_command name=midi action=in data="90 3C 64"    # as if it arrived (plays the synth)
    exec_command name=midi action=out data="B0 07 64"   # send bytes
    exec_command name=midi action=clock on=0|1          # clock, start and stop out
    exec_command name=open_midi                         # the window: what arrives and leaves, clock runs folded

## Residents jam

A resident answers the person's last phrase on the synth (`app/sound/jam.cpp`). It reads the person's gestures with
`person_events` through its own hands; when the person has played the synth and then stops for `wait_ms`, it reads
the phrase with `hear` (the person's notes since its last answer, at most 16) and plays an answer on the synth as
itself, in the person's rhythm (the median gap between their notes, 90-600 ms).

- **By rule, with no model:** `answer` (the phrase backwards, a fifth up: C D E F becomes C-5 B-4 A-4 G-4), `echo`
  (an octave up), `invert` (mirrored round the first note).
- **With a model present** (`WWDOS_JAM_MIND=<cli>`, else `claude` on PATH; `model=off` keeps to the rule), a
  confined call chooses the notes: `--tools ""`, `--strict-mcp-config`, `--setting-sources ""`, a fixed system
  prompt, haiku, run from `/tmp`, the phrase inside a `<DATA>` block. Only the first line of the reply that is
  nothing but notes (at most 16) is used; anything else, or no answer within `WWDOS_JAM_MIND_WAIT_MS` (8000), and
  the rule plays instead. The reply never runs a command.

    exec_command name=jam action=start as=wib|wob|scramble mode=answer|echo|invert wait_ms=700 model=auto|off
    exec_command name=jam action=state     # on, as, mode, model, thinking, answers [{phrase, answer, chose, step_ms, at_ms}]
    exec_command name=jam action=stop

The answer is the resident's act in the log (`synth action=notes`), and the ears name it as the resident's. The
resident's own notes never start an answer.

## Ears

The synth and the TR-808 tell the ears every note and hit as the engine plays it: the window, the voice, the note
or step, the level, and who played it (the synth) or who started the pattern (the TR-808). The ears hear with the
sound on or off, so a silent desktop in a test hears the same as a loud one. They keep the last 4096 sounds in
memory, across closed windows.

    exec_command name=hear [seconds=4] [of=all|synth|tr808] [row_ms=125] [window=w3]

(`of=`, because `source=` names the caller's standing.) The answer is marked `"standing":"data"`, changes
nothing and is not logged as an act.

    {"standing":"data","now_ms":48210,"seconds":"2.00","source":"all","row_ms":125,
     "sounding":[{"source":"synth","window":"w1","voice":"fm","note":"E-4","midi":64,"patch":"bass",
                  "level":0.51,"left_ms":160,"who":"wib"}],
     "heard":[{"ago_ms":1210,"source":"tr808","window":"w2","voice":"bd","step":0,"accent":true,"tempo":120,
               "level":1.13,"who":"wob"},
              {"ago_ms":200,"source":"synth","window":"w1","voice":"fm","note":"C-4","midi":60,"patch":"bass",
               "dur_ms":150,"level":0.79,"who":"wib"}, ...],
     "dropped":0,"counts":{"synth":3,"tr808":14},
     "summary":"in the last 2 s: synth 3 notes (C-4 to G-4) by wib 3; tr808 14 hits (bd 4, ch 8, sd 2) at 120 bpm, started by wob. Sounding now: E-4 fm (wib).",
     "meter":"▃▅█▆▃▂▁▁▂▅█▆▃▂▁▁",
     "rows":["-2.000   ---             | BD CH", "-1.875   ---             | CH", ...]}

- `sounding`: what sounds at this moment, with its level now (the synth by its envelope, a drum decaying over its
  ring: bd 400 ms, sd 200 ms, cy 900 ms, ch 60 ms ...) and how long it has left.
- `heard`: what started in the window of time, oldest first, each with `at_ms`, the clock time it sounds (the
  time it was asked for plus the stream's lead) (at most 400; `dropped` counts the rest).
  `level` for the synth is velocity times the patch level (0-1); for the TR-808 it is the hit's playback volume
  (`afplay -v`, 1.0 = sample level; accents go above 1).
- `rows`: one tracker row per `row_ms`, seconds ago on the left, synth notes, then drum voices.
- `meter`: one character per row, the summed level of everything sounding in it (` ▁▂▃▄▅▆▇█`).

The person sees the same meter: the synth window's `ears` line shows the last 4 s of every source.

The synth and score windows follow the describe contract (`app/describe.h`): `describe` gives their sentence and, as text, the last notes (synth) or the sections (score).

## Event scores

A score is a short text file that a person and a resident perform together. Each section has a length in bars,
who it is for, and an open instruction. The machine keeps time on the app's clock, shows the instruction now and
the bar and beat, listens through the ears, and acts itself only on `do` lines. The worked example is
`scores/call-and-answer.score`.

### Format

    # a comment
    title Call and answer
    tempo 120                 # beats per minute, 30-600
    beats 4                   # beats in a bar (default 4)
    part person the person at the console
    part wib a resident

    1 all     listen: the drum machine counts you in
    do tr808 preset electro
    do tr808 play
    4 person  play a phrase on the synth, four to eight notes
    4 wib     answer the last phrase
    2 all     rest two bars
    do tr808 stop

- A section line is `<bars> <who> <instruction>`: 1-64 bars, `who` a part named before it or `all`.
- An instruction that starts with `rest` is a rest; one that holds `answer` is an answer; any other is open.
  The machine does not judge how an instruction is followed. It reports what it heard in each section, whether a
  rest was kept (no synth notes in it), and, during an answer, the notes of the section before.
- `do` lines run when the section above them starts, from this list only: `do tr808 play`, `do tr808 stop`,
  `do tr808 preset <name>`, `do tr808 tempo <bpm>`, `do synth voice pcspk|fm`, `do synth patch <name>`.
  Any other line is refused with its line number before a window opens. Score text is data: it is shown and
  timed, never sent to a model and never run as a command.

### Commands

    exec_command name=open_event_score score=call-and-answer [start=1] [x y w h]   # reads scores/<score>.score
    exec_command name=open_event_score text="title ...\n1 all listen\n..."
    -> {"ok":true,"id":"w5","title":"Call and answer","sections":8}

    exec_command name=event_score action=start|stop|state [id=w5]

(`score=`, because exec_command's `name=` is the command's own name. With neither `score=` nor `text=` it opens
`scores/call-and-answer.score`.) A score file is looked for in
`$WWDOS_REPO/scores`, `./scores` and the source tree's `scores/`; the name is letters, digits, `-` and `_`.

The window shows the sections with the one now marked, what was heard in each (`[4 notes: person 4]`, `[kept]`),
the instruction now and the beat. Enter starts and stops it. `look_glance`: `event score "Call and answer",
running: section 2 of 8, bar 1 of 4; now person: play a phrase on the synth, four to eight notes`.

`event_score action=state` while running:

    {"id":"w5","ok":true,"standing":"data","title":"Call and answer","from":"scores/call-and-answer.score",
     "tempo":120,"beats":4,"bar_ms":2000,"running":true,"done":false,"started_by":"wib",
     "parts":[{"name":"person","is":"the person at the console"},{"name":"wib","is":"a resident"}],
     "section":3,"of":8,"bar":1,"bars":4,"beat":2,"ms_left":7400,
     "now":{"who":"wib","text":"answer the last phrase","kind":"answer"},
     "next":{"who":"all","text":"rest two bars","in_ms":7400},
     "answering":{"section":2,"notes":["C-4","D-4","E-4","F-4"],"by":"person 4"},
     "sections":[{"n":1,"bars":1,"who":"all","text":"listen: ...","kind":"open","state":"done",
                  "do":["tr808 preset electro","tr808 play"],"heard":{"notes":[],"by":{},"hits":7}}, ...],
     "acts":["tr808 preset electro: ok","tr808 play: ok","synth patch bass: ok"]}

### Who did what

The score acts as `score` (source app). Each section start is an act (`score "Call and answer" section 2 of 8:
person: play a phrase ...`), each `do` line is an act with its result, and so is the finish. Starting and
stopping it is logged with whoever did it. The ears name `score` as the one who started the drum machine when a
`do` line started it.

### A resident performing

A resident performs a score with the commands it already has: `event_score action=state` for the instruction and the
phrase to answer, `hear` for what sounds, `synth action=notes` (or its own keys to the synth window) to play.
Nothing in the score runs the resident; it decides when and what to play.

## Scramble and the chat

Scramble's paws (`pawCommandAllowed` in `app/resident_hands.cpp`) include `open_synth`, `synth` and `hear`, and
her prompt names them: an `ACT: synth action=notes notes="E-5 . C-5 G-4" patch=chip` line in her reply plays as
`scramble`, and `ACT: hear seconds=2` reads the ears. Her model call still carries no tools. The Wib&Wob chat
reaches every registry command through `tui_menu_command` (`app/llm/sdk_bridge/mcp_tools.js`), so it plays and
hears as wib or wob with no change; `tui_list_commands` lists `synth`, `open_synth` and `hear`.

## Tests

`app/sound/sound_test` (ctest `sound`) renders to buffers and needs no desktop: synth (17: note names, the PC
speaker's divisor and pitch measured by zero crossings, one speaker voice, FM level, release and silence, patches,
nine channels, envelope, WAV), hold (4: a held note sounds until it is let go, rings through its release, then silence; a key let go before its note starts gives a short note), mixer (7: the same samples in blocks of 1, 64 and 4096 and in the offline render, a
later call's speaker note replacing the one sounding, ten calls holding nine FM channels, a drum hit on its exact
sample, late counting, capture), patterns (8: the first format, patterns and order, the text round trip, F tempo and speed, refused orders and numbering, F00), jam (6: the three rules, a model's line of notes taken, anything else refused), ears (16: order, sounding now, windows of time, levels, rows, meter, summary,
report, window filter, the keep limit), score (14: parsing, kinds, timing, and seven refused scores).

`python3 tests/resident/test_machine_sound.py [patterns] [jam] [midi] [tracker] [drums] [synth] [ears] [score] [hands] [stream]` starts its own silent desktop
(`tmux -L soundtest`, `WWDOS_SOUND_OUT=null`, `WWDOS_MIDI=null`, `WWDOS_SYNTH_AUDIO=0`, `WWDOS_TR808_AUDIO=0`, no claude CLI on PATH). `WWDOS_BIN` picks
the binary.

| group | what it shows |
|---|---|
| patterns | the first file format loads as one pattern; a song of patterns 0, 1, 0 plays in order and the ears give back its ten rows; rows are 62.5 ms at 240 bpm until `F78` sets 120 bpm, then 125 ms; muting the drums leaves only the bass, soloing them only the drums; a 2x2 block copies and pastes into pattern 1, a drum column on a note channel changes nothing; the person's `>`, Ctrl+E and Ctrl+T edit pattern 1, mute and solo; the text holds the order and both patterns |
| jam | wib answers the person's C D E F with C-5 B-4 A-4 G-4 by rule, in their rhythm (160 ms), the ears and the log name wib, wib's own notes start no answer, the next phrase gets its own answer; in the hands desktop a stand-in model chooses wob's answer (`G-4 . E-4 C-4`) through a call with no tools, no MCP and no settings and the phrase as data, and a reply that is not notes (`ACT: close_all`) falls back to the rule and closes nothing |
| midi | a note on plays C-4 as the person's and holds an FM channel; the note off ends it after as long as it was held (measured 412 ms for a 400 ms hold) and frees the channel; velocity 0 is a note off; the log has the person's note; the TR-808 sends start first, stop last, clock ticks 20.833 ms apart (24 a beat at 120 bpm) and each bass drum as note 36 on channel 10 at the time the ears say it sounds; the tracker sends C-4 on channel 1 and its note off 62.5 ms later, its drum on 10 and 6 ticks a row; the MIDI window says what arrived and left |
| tracker | the person's z x c write C-4 D-4 E-4; wib writes drums and a bass cell by command, wob an arpeggio cell with its own keys (note, then `a37` in the effect field); the log names all three; save writes `tracks/<name>.trk` as plain text and open_tracker loads the same rows; a name with `..` is refused; one pass: the ears' `tracker_rows` equal the tracker's rows that sounded, and the stream mixed the 7 notes; with loop on it plays round |
| drums | a new TR-808 is empty; play loads classic-house, the stream counts the hits it mixed and the ears hear them; a pattern with one step on plays as it is |
| synth | wib opens the synth (owner wib, a glance sentence); wib's phrase by command, the person's C D E from z x c, Tab to the PC speaker and `]` to octave 5, and wob's key through `resident_key` each play and name their player; nothing reaches the speakers when silent (sent 0); the acts log has all three; `render` of the PC speaker's A-4 is 22050 samples with 439 upward zero crossings, the same twice, and plays nothing |
| ears | silence first; right after wib's phrase starts, C-4 by wib is sounding; wob then hears C-4 E-4 G-4, 200 ms apart, as played, with a summary and 16 rows; with the TR-808 started by wob, every hit heard is a step the engine's pattern has on, names wob and 120 bpm; `window=` filters; hear is not logged; the synth window shows the meter |
| hands | on a desktop of its own with a stand-in claude CLI: the capabilities list synth, open_synth and hear; Scramble's ACT line plays a tune on the synth as scramble, her prompt names the commands and her call still has no tools, the log and the ears name her; the chat's socket client (`app_socket.js`) plays and hears as wob |
| stream | the null stream runs and its clock follows real time; with the synth mixed in, each of wib's four notes is in the captured mix within 3 ms of the ears' `at_ms` (measured 0.0-0.1 ms), 150 ms apart on the clock to 0.1 ms; the person's four keys likewise; ten notes from ten commands hold nine FM channels; nothing late; every TR-808 onset in the mix is within 3 ms of a step and the steps are whole 16ths apart; a score at 240 bpm with the drums: every step 1 falls on a bar line of the score (worst 0.04 ms over 6 bars) and the drums stop with the score |
| score | wib starts a score at 480 bpm; the person plays C D E F in their section; in wib's answer section the score hands wib those four notes; wib answers; the rest is kept; the score finishes on the clock; each section's notes are attributed (person 4, wib 4), the drum machine played under the person and stopped when the score said; the log has the sections, the `do` lines and the finish by `score`; a `do shell` line, a command line and a path outside `scores/` are refused and no window opens; the worked example opens with 8 sections |

On origin/main (before these commits) every group fails at its first sound command (`err unknown command`, or the animated Score window's plain `ok` for `open_score`, which is why the event score commands are named `open_event_score` and `event_score`).

## What is weak

- **The lead is fixed at 46 ms.** Every sound asked for now plays 46 ms later. It keeps keyboard playing even and
  never late, but a player hears the delay; a smaller AudioQueue buffer would cut it at the risk of drop-outs, which
  are not counted.
- **No key releases from the computer keyboard.** A terminal sends key presses only, so a typed note lasts the gate length; a MIDI keyboard gives real releases.
- **MIDI out goes to the virtual source only.** A DAW or another app listening to `WibWob-DOS` gets it; a hardware synth needs that routed (Audio MIDI Setup). Input ignores everything but notes (no controllers, no pitch bend), and no MIDI clock is followed.
- **Mac only for sound.** The stream plays through an AudioQueue; elsewhere it runs `null`. The browser build is not
  wired and stays silent.
- **The ears hear the schedule, the capture hears the mix.** `hear` reports notes and hits as scheduled; only
  `sound action=capture` looks at the samples, and its onset finder misses quiet hits right after loud ones (the
  test asks for most steps, not all). A TR-808 level is its playback volume, not a loudness.
- **The clock is the stream's samples as rendered,** extrapolated between blocks; it does not read the device's own
  play position, so the true output can sit a buffer (35 ms) behind what the clock says.
- **The ears keep their history in memory only** (4096 sounds). There is no event stream: a resident polls `hear`.
- **Scores report but do not judge.** "Answer the last phrase" gets the phrase and who played what; only rests are
  checked, and only for synth notes. Two score windows can both drive the TR-808.
- **No tracker editing.** The ears print tracker rows; nobody can write music in them yet.
