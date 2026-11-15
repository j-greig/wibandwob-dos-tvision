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

### Sound out

A phrase is rendered to one 16-bit mono WAV at 22050 Hz in `$TMPDIR/wwdos-synth-<pid>/` and played with
`afplay` on the TR-808's player thread (`tr808::playFile` in `app/tr808_audio.cpp`), so the synth and the drum
machine share one audio path. The files are removed when the app exits normally.

`WWDOS_SYNTH_AUDIO=0` starts the synth silent; `action=audio on=0|1` and the `0` key switch it. Silent, notes
are still played, logged and heard by the ears; `sent` and `silenced` count where they went. Tests run silent.

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
- `heard`: what started in the window of time, oldest first (at most 400; `dropped` counts the rest).
  `level` for the synth is velocity times the patch level (0-1); for the TR-808 it is the hit's playback volume
  (`afplay -v`, 1.0 = sample level; accents go above 1).
- `rows`: one tracker row per `row_ms`, seconds ago on the left, synth notes, then drum voices.
- `meter`: one character per row, the summed level of everything sounding in it (` ▁▂▃▄▅▆▇█`).

The person sees the same meter: the synth window's `ears` line shows the last 4 s of every source.
`look_glance` now gives the TR-808 a sentence too: `TR-808 drum machine, playing pattern A1-A at 120 bpm, step 5
of 16; 4 instruments programmed; sound off`.

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

(`score=`, because exec_command's `name=` is the command's own name.) A score file is looked for in
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

## Tests

`app/sound/sound_test` (ctest `sound`) renders to buffers and needs no desktop: synth (17: note names, the PC
speaker's divisor and pitch measured by zero crossings, one speaker voice, FM level, release and silence, patches,
nine channels, envelope, WAV), ears (16: order, sounding now, windows of time, levels, rows, meter, summary,
report, window filter, the keep limit), score (14: parsing, kinds, timing, and seven refused scores).

`python3 tests/resident/test_machine_sound.py [synth] [ears] [score]` starts its own silent desktop
(`tmux -L soundtest`, `WWDOS_SYNTH_AUDIO=0`, `WWDOS_TR808_AUDIO=0`, no claude CLI on PATH). `WWDOS_BIN` picks
the binary.

| group | what it shows |
|---|---|
| synth | wib opens the synth (owner wib, a glance sentence); wib's phrase by command, the person's C D E from z x c, Tab to the PC speaker and `]` to octave 5, and wob's key through `resident_key` each play and name their player; nothing reaches the speakers when silent (sent 0); the acts log has all three; `render` of the PC speaker's A-4 is 22050 samples with 439 upward zero crossings, the same twice, and plays nothing |
| ears | silence first; right after wib's phrase starts, C-4 by wib is sounding; wob then hears C-4 E-4 G-4, 200 ms apart, as played, with a summary and 16 rows; with the TR-808 started by wob, every hit heard is a step the engine's pattern has on, names wob and 120 bpm; `window=` filters; hear is not logged; the synth window shows the meter |
| score | wib starts a score at 480 bpm; the person plays C D E F in their section; in wib's answer section the score hands wib those four notes; wib answers; the rest is kept; the score finishes on the clock; each section's notes are attributed (person 4, wib 4), the drum machine played under the person and stopped when the score said; the log has the sections, the `do` lines and the finish by `score`; a `do shell` line, a command line and a path outside `scores/` are refused and no window opens; the worked example opens with 8 sections |

On origin/main (before these commits) every group fails at its first sound command (`err unknown command`, or the animated Score window's plain `ok` for `open_score`, which is why the event score commands are named `open_event_score` and `event_score`).

## What is weak

- **Latency.** Each phrase is a WAV file handed to `afplay`, which takes tens of milliseconds to start. Fine for
  a phrase from a command; a person playing fast from the keyboard hears each note late and unevenly. A real
  audio stream (CoreAudio, or SDL) would fix it.
- **Overlapping phrases are separate processes.** The PC speaker is one voice inside a phrase, but two phrases
  sent close together play over each other, and FM's nine channels are counted per phrase, not across them.
- **No key releases.** A terminal sends key presses only, so every keyboard note lasts the gate length.
- **Mac only.** Sound goes out through `afplay`. The engine is plain C++ with no dependencies and could feed a
  Web Audio buffer in the browser build, but that is not wired; the browser desktop is silent.
- **The ears hear what was played, not what came out.** They take the engines' own notes and hits; they do not
  measure the audio (no onsets from the signal, no mixing of the drum samples). A TR-808 level is its playback
  volume, not a loudness.
- **Ears are in memory.** 4096 sounds, lost on restart. No event stream: a resident polls `hear`.
- **Scores do not check instructions.** "Answer the last phrase" is reported (the phrase, who played what), not
  judged. Only rests are checked, and only for synth notes. One score runs per window; nothing stops two windows
  running scores that both drive the TR-808.
- **No tracker editing.** The ears print tracker rows; nobody can write music in them yet.
- **Score timing is the window's timer** (20 ms), not sample-accurate, and it does not share a clock with the
  TR-808: a score's bars and the drum machine's steps can drift apart over a long score.
