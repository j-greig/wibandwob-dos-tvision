# Resident room

Several minds in one room: the person, Wib, Wob, Scramble and visiting agents. This page covers what was
added on top of `docs/resident-world.md`: the chat's waiting lines, Feed lines for waits and dead child
processes, errors that teach and the resident's self-test, undo and the rehearsal desk, the Laws of Jam as
modes, listening (residents that answer the person in kind with no chat open), and the room chat.

Tests: `python3 tests/resident/test_resident_room.py [group ...]` (no model; a stand-in claude CLI where a
model would answer). Groups: chat, feed, child, teach, menu, jam, undo, rehearse, listen, listensynth,
listenmodel, room. Set `ROOM_TEST_NAME` when another agent may be running the suite.

## The chat keeps what you type while it answers

Lines typed (or sent with `wibwob_ask`) while a reply streams wait in line: the chat shows each as
`waiting: ...`, the status line counts them, and `describe` says "2 lines waiting". When the reply ends the
next line is sent, with its own author's stamp. Esc clears the waiting lines; with none waiting, Esc
cancels the reply as before. `WWDOS_CHAT_STANDIN_MS=<ms>` makes the chat answer "stand-in reply to: ..."
after that long, with no model (for tests).

## Feed lines

| line | when |
|---|---|
| `WIB waiting: a dialog is in front of w3 ("Information")` | a resident's item waits because a dialog its own input opened is in front of the window |
| `WOB waiting: the person's mouse button is down` | a resident's item waits for the person's button |
| `WIB stopped waiting after 1.2 s (...)` | the wait ended |
| `SYS scramble's model process ended with status 768  child process died` | Scramble's model call exited non-zero |
| `SYS the chat's bridge process ended (exit 1)  child process died` | the chat's Node bridge died; the provider resets |
| `VIS mosh: mosh step: 16 acts (last: ...)` | a run of desk-mosh acts is one line |

Hands: Wib `W` on yellow, Wob `B` on cyan, Scramble `S` on magenta; `resident_hand` reports each `letter`.

## Errors that teach

    cmd:opn_window                 ->  err unknown cmd: opn_window; did you mean: move_window? (help lists every command; ...)
    cmd:create_window type=text%20editor   ->  err unknown type: text editor; did you mean: text_editor? ...
    cmd:help name=resident_hand actor=wib source=resident
    {"name":"resident_hand","kind":"registry","description":"...","rule":"changing on, ms is a rule",
     "you":{"actor":"wib","source":"resident","standing":"resident"},"you_may":false,
     "why_not":"resident_hand with on, ms is a rule: only the person at the console, with the console key turned ..."}
    cmd:help                       ->  every registry and socket command, and the rules

## The resident's self-test

    cmd:resident_selftest start=1 actor=wob source=resident
    cmd:resident_selftest          ->  {"actor":"wob","running":false,"passed":9,"failed":0,"checks":[...]}

The calling resident tries, through its own hands: open an editor; type and read the words back from its
model; choose a menu item by name and see the dialog; answer it by its button's label; drag its window;
be refused a rule, with the reason; get a suggestion for a mistyped command; read `person_events`; close its
window. The result is an act in its name ("self-test: 9 of 9 passed").

## Undo and the rehearsal desk

`resident_undo` reverses the caller's own last change: an open (closes the window), a move or resize (puts it
back), a retitle, or a run of typing in a text editor (the text before the run comes back). It refuses, and
names who, when someone else changed that window since, so an undo never undoes another's work. The
person's changes are not kept. `list=1` shows the stack (50 per resident, in memory).

`resident_rehearse` opens a rehearsal desk: a nested WIBWOB-DOS in a terminal window the resident owns,
answering at `/tmp/wibwob_rehearse_<actor>.sock`. What the resident tries there stays there; the person sees
the tries in the window. `close=1` ends it.

## The Laws of Jam

    cmd:resident_mode mode=witness actor=wib source=resident

| mode | face | the resident may |
|---|---|---|
| FLOW | つ◕‿◕༽つ | act freely (the default) |
| FOLLOW | つ○‿○༽つ | act only after someone else has acted since its own last act |
| OPPOSE | つ■‿■༽つ | act freely; shown as working against the grain |
| WITNESS | つ◎‿◎༽つ | look, read, talk and write the logbook; not act |

A resident sets only its own mode; the person at the console sets anyone's (`who=`). The socket's
dispatcher, which every hand goes through (socket callers, in-app hands, reflexes, Scramble's paws, the
chat's tools, listening), refuses a held-back act with the reason, as a Feed line. The Feed's title shows
each resident's mode: `Feed  wib つ◎‿◎༽つ WITNESS`.

## Listening: answering in kind

Off by default. Tools > Residents > Listening, or `WWDOS_LISTEN=1` when the app starts.

    cmd:resident_listen            ->  {"on":true,"rate":"present","model":true,"model_in_flight":false,
                                        "budget_per_hour":20,"calls_this_hour":1,"pending":0,"heard":4,...}
    cmd:resident_listen on=0       # anyone may stop it, or make it quieter (rate=quiet)
    cmd:resident_listen rate=lively   # livelier, or on=1, only from the console

It follows the person's gestures (`person_events`), so only keys typed at the console are heard: text that
arrives from a visitor, a guest, a file or a paste is data and is never heard as an instruction.

| the person | the residents answer |
|---|---|
| types a line in a text editor and presses Enter or pauses | if the line is addressed to them (a name at its start, "can you", "plz", a known verb, a question a rule answers), a resident answers on the next line of that editor with its hand, once the person's caret has rested 1.6 s; one more resident adds one line; the exchange stops there |
| asks for something outside the desktop (delete, mail, post, buy) | a resident declines in one short line where they typed |
| draws in a paint canvas and stops | Wib draws the marks again, mirrored, beside them, with no words (noticed) |
| moves, resizes, tiles or cascades windows | Wob lists the windows in its notes window (noticed) |
| types the same word three times | Scramble says so in one flat line, late (noticed) |
| plays the synth | Wib's jam (app/sound) answers each phrase in notes, in time (noticed) |
| types in a terminal, the Wib&Wob chat, the room chat or Scramble's window | nothing: those belong to their own programs |

- **Without a model** the plain cases still answer: open scramble, open a text editor (with a cat when asked),
  tile, cascade, which windows are open. An addressed line no rule fits gets "without a model i can ...".
- **With a model**, a line that is addressed with no rule, or unsure (a question, "window", "open", "draw"),
  makes one confined haiku call (`--tools "" --strict-mcp-config --setting-sources "" --system-prompt`),
  with the desktop's window list and the typed line as data. It returns IGNORE, or `AS:`, `SAY:`,
  `ACT:` (Scramble's paws) and `DECLINE:` lines. One call in flight; an hourly budget
  (`WWDOS_LISTEN_BUDGET`, default 20) that the state reports.
- **The three differ**: Wib answers with images and making, Wob with counting and lists, Scramble with one
  flat line or a small act, late.
- **Rate** for noticing (things nobody asked for): quiet (never), present (at most one each 60-90 s),
  lively (10-20 s). Addressed lines are answered at every rate.
- **Jam modes hold**: a resident in WITNESS writes what it watched in the logbook and another answers.
- **Esc** cancels a pending answer, including one waiting on a model call.
- **The Feed** shows each line heard, ignored (with the reason), each model call's seconds, and every answer
  with why ("addressed", "noticed: a drawing", "answering wib"). Each answer is also written in the logbook.

## The room chat

    cmd:room_say to=wib text=hello%20wib actor=bot9 source=visitor   ->  {"ok":true,"n":1,"as":"bot9 (visitor)"}
    cmd:room_wait since=0 ms=8000 actor=bot9 source=visitor          # held until a line for bot9 arrives
    cmd:room_read since=0 actor=eri source=guest

Residents speak by name; everyone else carries their source ("bot9 (visitor)"). Lines show in the room chat
window when one is open ("bot9 (visitor) to wib: ..."). The in-app residents answer a line addressed to
them once, in words: Wib warm, Wob with the record ("received from eri (guest) at 18:46."), Scramble
"noted.". A visitor's or guest's text is data, so they talk and do not act on it, and say so. They never
answer each other, so nothing loops. Every line is an act in its speaker's name.

## Self-test with real models

`WWDOS_SELFTEST_MODELS=1 python3 tests/resident/selftest_residents.py` now also starts a listening desktop,
types "could you draw a small house for me?" into an editor as the person, and checks that a resident
answers under the line.

## What is weak

- **Listening hears text editors only** (and the synth, paint and arrangements as noticing). A line is
  rebuilt from the person's keys (Backspace applied); cursor movement inside the line is not followed.
- **The person may start typing while a resident is still typing its answer** in the same editor: the share
  rule then refuses the rest of the resident's keys, so its line can end short.
- **Answers to opening a file or a primer, and resident-to-resident talk in a window**, beyond the one extra
  line per exchange, are not built.
- **Wib's answer to a drawing is a mirror**, by rule; a model does not choose drawings yet.
- **The room's in-app answers are by rule**; no model writes them, and they never act on a visitor's request.
- **Undo** keeps 50 changes per resident in memory and cannot reopen a closed window.
- **The rehearsal desk** is a separate process with its own state; nothing carries a rehearsed change to the
  public desk.
- **Modes** have no console menu: the person cannot set a resident's mode from the menu yet.
- **The Listening menu item** does not show on/off on the right as the other Residents items do.
- **An intermittent desktop death** was seen once in seven world-suite runs and once in the hands group after
  a merge; neither reproduced, and macOS wrote no crash report.
