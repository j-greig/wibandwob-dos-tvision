# Resident life

The residents (Wib, Wob and Scramble) keep a self on disk, wake as a boot a person can watch, leave a note for the
next waking, keep a logbook and read a guestbook, and sleep when nobody is there. These sit beside resident input
(`docs/resident-input.md`) and resident senses (`docs/resident-senses.md`): those give a resident hands, eyes and a name on
what it does; this gives it a life that continues from one waking to the next.

Everything here works with no model. The boot screen, the notes written at closing, the logbook entries, the dreams
and the tidying are written by rule from real state: the acts log, the windows and their owners, the files on disk.
Nothing in this module calls a model. A resident that runs on a model elsewhere (an agent driving the socket) writes
its own note and log entries through the commands below.

Code: `app/resident_life.cpp`, `app/resident_life.h`. It joins the app in `TWwdosApp` (constructor: `life::wake()` last;
`run()`: `life::close()` after the run loop; `getEvent`: `life::consoleInput()` for the person's own keys and clicks;
`idle()`: `life::tick()`). Tests: `tests/resident/test_resident_life.py`.

## Where the self lives

One directory, plain text files:

    $WWDOS_HOME            (default ~/.wwdos; shown in the fiction as C:\WWDOS)
      SELF.INI             wakings: count, last waking, last close, clean or running
      DESK.TXT             windows open at the last close, with owners

The directory is created on first use. Files are rewritten whole through a temporary file and a rename, so a
crash leaves the previous version. Two desktops started with the same `WWDOS_HOME` share one self.

### SELF.INI

    [self]
    boots=5
    last_wake=SUN 15-11-2026 17:02
    last_close=SUN 15-11-2026 17:40
    closed=clean

`closed=running` is written at waking and replaced by `clean` when the app ends through Exit (File > Exit, Alt+X, the
status line). A waking that is killed (the tmux session killed, the power lost) leaves `running`, and the next boot
says so. Dates are in the form a period directory listing used: day, DD-MM-YYYY, HH:MM, local time.

### DESK.TXT

    ; id<TAB>type<TAB>owner<TAB>x,y,w,h<TAB>title (front window first)
    w2	text_editor	wib	4,3,40,10	Plans
    w3	test_pattern	person	2,1,48,14

Owners are the authors that opened each window (resident senses). Windows are recorded, not reopened.

## Waking: the boot screen

At start the desktop opens a `boot` window in front and types its lines one at a time, as a BIOS power-on self-test
did. Every line comes from the self on disk:

    WIBWOB-DOS BIOS  Version 1.0   (C) Wib&Wob
    Memory test: 640K OK
    Self on disk: C:\WWDOS  = /Users/james/.wwdos
    Waking 5.  SUN 15-11-2026 17:02
    Last waking: SUN 15-11-2026 16:10, closed cleanly SUN 15-11-2026 16:55.
    DEVICE=EYES.SYS ...... look_glance look_read look_study look_changes
    DEVICE=HANDS.SYS ..... resident_key resident_mouse resident_press resident_menu
    DEVICE=VOICE.SYS ..... no model; the residents speak by rule
    WIB .......... 1 window was mine at the last close
    WOB .......... no windows of mine at the last close
    SCRAMBLE ..... no windows of mine at the last close
    Windows at the last close: 2
      w2    text_editor    wib       4,3 40x10  "Plans"
      w3    test_pattern   person    2,1 48x14  ""

    Human not present. Press any key to continue

After a killed waking the third line reads `WARNING: the last waking (...) did not close.` The person's first key or
click during the boot shows the remaining lines at once; the next closes the boot screen. That key is used up: it
does not reach the window behind. With nobody there the boot screen closes itself `WWDOS_BOOT_HOLD_MS` after its last
line. `look_glance` says `boot screen of waking 5, 13 of 16 lines shown; last line: "..."` and `look_read` gives the lines shown.

Each resident's waking is logged as an act (`woke: waking 5`, actor wib, wob, scramble, source resident); the close is
logged as an app act.

### get_self

    cmd:get_self

    {"home":"/Users/james/.wwdos","waking":5,"woke":"SUN 15-11-2026 17:02","found":true,
     "last_wake":"SUN 15-11-2026 16:10","last_close":"SUN 15-11-2026 16:55","last_closed_cleanly":true,
     "desk_at_last_close":[{"id":"w2","type":"text_editor","owner":"wib","x":4,"y":3,"w":40,"h":10,"title":"Plans"}],
     "boot":{"open":true,"id":"w1","shown":16,"lines":16}}

### Settings

| variable | default | what |
|---|---|---|
| `WWDOS_HOME` | `~/.wwdos` | the self's directory |
| `WWDOS_BOOT` | on | `0` opens no boot screen (the waking is still counted) |
| `WWDOS_BOOT_MS` | 120 | time between boot lines; 0 shows them all at once |
| `WWDOS_BOOT_HOLD_MS` | 20000 | the boot screen closes this long after its last line; 0 keeps it until a key |

`tests/resident/resident_lib.py` starts every desktop with its own `WWDOS_HOME` under /tmp, no boot screen and no sleep, so
the other resident suites see a desktop with no windows of its own.

<!-- sections -->

## Tests

`python3 tests/resident/test_resident_life.py [boot] [note] [book] [sleep]` starts its own desktop (`tmux -L lifetest`) with
a private self directory (`/tmp/wibwob_lifetest.self`), quits it with Alt+X through tmux, kills it, and starts it again,
without the claude CLI on PATH and without `ANTHROPIC_API_KEY`.

| group | what it shows |
|---|---|
| boot | the first waking is waking 1 with no self; a boot screen opens and look_glance says what it is; its lines appear and end with the BIOS prompt; SELF.INI says running; the first key closes the boot and is not typed into the window in front; Exit writes closed=clean and DESK.TXT with wib's editor and the person's window and their owners; the second waking reads both back and shows them on the boot screen by resident and title; a killed waking is warned about at the next |

## Weak

- Windows are recorded at the close and named at the boot, not reopened.
- A desktop that is killed writes nothing at all: no desk, no notes.
