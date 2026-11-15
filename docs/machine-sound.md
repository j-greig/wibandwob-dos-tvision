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
