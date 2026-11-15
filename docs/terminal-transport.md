# Terminal transport

A program running in a terminal window (a film started with `node timelag.mjs`, a shell job, anything) can be
paused and resumed by the person at the desk and by a resident, with the same effect, and the window shows that
it is paused. A terminal window's output can be recorded as a cast, and a cast player window plays a cast back
with no program running.

Code: `app/tvterm_view.cpp` (process groups, title, frame mark, recording hook), `app/terminal_transport.cpp`
(registry commands, act log), `app/cast_record.cpp` (cast files), `app/cast_player.cpp` (player window).
Tests: `tests/resident/test_terminal_transport.py`.

## Pause and resume

Pause sends SIGSTOP to every process group on the terminal's session: the shell or exec program, the job it
runs and anything that job started that shares the terminal (an audio player started by a film, a pipeline).
Picture and sound both hold. Resume sends SIGCONT in the reverse order, the job before the shell, so an
interactive shell does not see its job stop and take the terminal back.

    pause:  shell ──STOP──▶ job ──STOP──▶ audio player        (session leader first)
    resume: audio player ◀──CONT── job ◀──CONT── shell        (reverse)

| who | how |
|---|---|
| person | Alt-P (a status-line key, so it works while the terminal holds the keyboard); a click on the frame mark `[‖]` (running) or `[►]` (paused) left of the zoom icon; Window > Pause or Play Terminal |
| resident | `terminal_pause`, `terminal_resume`, `terminal_toggle`, `terminal_state`; or `resident_key id=wN keys=Alt+P`, or a `resident_mouse` click on the mark |

The person's Alt-P and the Window menu entry act on the focused terminal, else the only terminal on the desk.

    cmd:terminal_pause id=w3          {"ok":true,"id":"w3","state":"paused","changed":true,"program":"node"}
    cmd:terminal_resume id=w3         {"ok":true,"id":"w3","state":"running","changed":true,"program":"node"}
    cmd:terminal_toggle               the focused terminal, else the only one; else "err several terminal windows: give id="
    cmd:terminal_state id=w3          {"id":"w3","state":"paused","program":"node","seconds_paused":12.4,"keyboard":"focus"}

- `state`: `running`, `paused` or `exited` (the program ended; with `hold=1` the window keeps its last screen).
- `program`: the name of the terminal's foreground process (`node`, `zsh`, `bash`).
- A second pause or resume answers `"changed":false`.
- The title adds `(Paused)`; `describe` says "node is paused (12 s), Alt-P or terminal_resume plays it".
- Every pause and resume is an act in the actor's name ("paused the terminal (node)", "resumed the terminal
  (node) after 12.4 s"), so the Feed shows who did it.
- Scramble's ACT lines may use `terminal_pause`, `terminal_resume`, `terminal_toggle`, `terminal_state`,
  `terminal_release`, `cast_control` and `cast_state`.
- No app timer ends a paused program. Closing the window sends SIGHUP and then SIGCONT, so a paused program
  still ends with its window.

Limits: a process that left the terminal's session (`setsid`, a daemon) is not paused. Audio already handed
to the sound device may play out its buffer after the stop.

A program that times its picture by the wall clock jumps ahead by the paused time on resume, while its audio
player continues from where it stopped, so the two drift apart by that time. The terminal films do this
(`const t = Date.now() / 1000 - t0` in `timelag.mjs`). Such a program stays in sync if it moves its clock on
SIGCONT by the time it was stopped:

    let last = Date.now();
    process.on('SIGCONT', () => { t0 += (Date.now() - last) / 1000; });
    // in the frame loop: last = Date.now();

A program that counts its own frames continues where it stopped with no change.

## The keyboard

A terminal window takes keys only while it is the focused window. F6, a click on another window, the menu bar
(F10, Alt+letter) and the status-line keys all work as usual.

Window > Grab Keyboard (Terminal) makes the focused terminal hold the keyboard: every key except the status
line's goes to its program, menus included. The title adds `(Input Grab)` and the status line shows:

    Alt-End Release keyboard   Alt-P Pause/Play

| who | gives the keyboard back |
|---|---|
| person | Alt-End, or a click outside the window |
| resident | `cmd:terminal_release id=w3` → `{"ok":true,"id":"w3","released":true,"keyboard":"focus"}` |

`terminal_state` reports `keyboard`: `grab` (the window holds the keyboard), `focus` (the focused window),
`none`. `describe` adds "it holds the keyboard (Alt-End or terminal_release gives it back)".

Every terminal window used to read "(Input Grab)" from the moment it opened: the app's tvterm constants were
listed by position and skipped `cmCopySelection`, so `hcInputGrabbed` was 0, the help context of every
view. The constants are named fields now; the title says Input Grab only during a grab.

## Casts

A cast is an asciinema v2 file: one JSON header line, then one `[seconds, "o", "text"]` line per piece of output.

    {"version": 2, "width": 38, "height": 11, "timestamp": 1794766735, "title": "bounce", "env": {...}}
    [0.000000, "o", "\u001b[0m\u001b[2J\u001b[H..."]
    [0.013806, "o", "\u001b[2J\u001b[H\u001b[?25l+----..."]

### Record

    cmd:terminal_record id=w3 action=start file=/path/out.cast
    cmd:terminal_record id=w3 action=stop      {"ok":true,"file":"...","events":50,"seconds":6.8}
    cmd:terminal_record id=w3                  {"recording":true,"file":"...","seconds":2.1}

- The first event (time 0) draws what the window shows when recording starts.
- Output read within 10 ms of the previous read joins one event.
- The recording's clock holds while the program is paused, so a paused stretch is not in the cast.
- The title adds `(Rec)` while recording; closing the window ends the recording.
- The cast's width and height are the terminal's inner size.

### Play

    cmd:open_cast file=casts/bounce.cast [x= y=] [speed=2] [loop=1] [paused=1]   {"ok":true,"id":"w4"}
    cmd:cast_control id=w4 action=play|pause|toggle seek=3.5 speed=0.5 loop=1
    cmd:cast_state id=w4     {"file":"...","width":38,"height":11,"events":50,"duration":6.5,"pos":3.5,
                              "playing":false,"speed":0.5,"loop":true}

The player (window type `cast_player`) feeds the cast to a libvterm screen (tvterm's `VTermEmulator`) as its
clock passes each event, and draws that screen as ordinary window cells, so `look_study`, `look_read`,
`window_text`, `describe` and the desk mosh read it like any window. Seeking back replays the cast from the
start. No child process runs.

| key | does |
|---|---|
| Space | play or pause |
| Left, Right | back or forward 1 s |
| Home, End | start, end |
| 0-9 | 0 % to 90 % of the cast |
| - and + | half or double speed (0.25 to 8) |
| L | loop on or off |

The control row under the screen is clickable: `[◄◄]` back 5 s, `[►]`/`[‖]` play or pause, `[►►]` forward
5 s, the position, `[-]` and `[+]` speed, `[once]`/`[loop]`.

### The example

`casts/bounce.cast` was recorded from `casts/bounce.sh` (a ball bouncing in a box for six seconds, 48 frames):

    cmd:open_terminal exec=read%20x;%20sh%20casts/bounce.sh;%20sleep%2030 x=2 y=1 w=40 h=13 title=bounce
    cmd:terminal_record id=w1 action=start file=casts/bounce.cast
    cmd:terminal_write window_id=w1 text=%0A        # the animation starts
    cmd:terminal_record id=w1 action=stop           # after it ends
    cmd:open_cast file=casts/bounce.cast

## Tests

    WWDOS_BIN=build-term/app/wwdos python3 tests/resident/test_terminal_transport.py [group ...]

Groups: `commands`, `shell` (a pipeline typed into an interactive shell), `person` (Alt-P, the frame mark, the
Window menu, a resident's click and key), `focus` (paused while focus moves away and back), `close` (closing
a paused window ends its processes), `keyboard` (grab, Alt-End, terminal_release), `casts` (record, seek to
chosen seconds and compare with the terminal's screen then, play, pause, keys, frame controls, loop, the
example). Its own desktop under `tmux -L termtest` (`TERM_TEST_NAME` to change it), no model.
