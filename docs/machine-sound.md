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
