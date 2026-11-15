# Resident world

The person at the console and the residents (Wib, Wob, Scramble, and visiting agents) share one desktop.
This page covers four things built on resident input (`docs/resident-input.md`) and resident senses
(`docs/resident-senses.md`):

- **describe**: every window says what it is, from its own model, so no agent reads text off the screen.
- **person events**: what the person does arrives as an ordered stream a resident can wait on.
- **residents in the machine**: Wib, Wob and Scramble act through the app itself, with hands that reach only
  the desktop; one command table serves the socket, the API server, MCP and the residents.
- **the Feed**: a window that shows the act log live, one line per act, tagged by who acted.

## Try it

    scripts/resident-world-demo.sh

The script starts `build-resident/app/wwdos` with `WWDOS_RESIDENTS=residents/demo.reflex` and the claude CLI
off PATH, so no model runs. Wib opens a window titled "residents" and Wob opens the Feed. Then:

| you do | the residents answer |
|---|---|
| open a window (Ctrl+N, or any menu item that opens one) | wib writes "you opened a test_pattern" in the residents window; wob's reading mark appears on the new window |
| type in a text editor of your own and pause | scramble writes back what you typed |
| close a window | wob notes which kind closed |
| open a menu | wib asks about it |

Each answer travels through the resident queue: the actor's hand (a black-on-yellow initial) glides to the
residents window before the keys land, and every act appears in the Feed. Your focus stays where it was.

## describe: every window says what it is

    cmd:describe id=w3
    {"id":"w3","type":"text_editor","class":"TTextEditorView","title":"notes","owner":"person",
     "x":4,"y":2,"w":50,"h":10,"focused":true,"seen":"all","contract":true,
     "state":"text editor holding 2 lines, 17 characters; first line: \"hello world\"; cursor at line 2, column 7",
     "standing":"data","text_from":"model","count":2,"text":["hello world","second"]}

    cmd:describe            # every window, front first; text=1 adds each window's text

- `state`: one sentence from the window's own model: a game's score and level, a player's file and frame
  count, a chat's message count and last sender, a terminal's last line, an animation's mode and frame.
- `text`: what the window holds in its model (`text_from: model`): editor lines, chat messages, terminal
  output, a paint canvas, a FIGlet window's words, a game's message log. A window whose model holds no text
  gives its cells (`text_from: cells`); a dialog gives its static text and input lines (`text_from: controls`).
  A window covered completely by another still answers with its text.
- `contract: false` means the window has no describer (its sentence then comes from its cells). The audit
  prevents that from shipping.
- `window_text id=` reads any window whose model holds text, not only the editor; `look_glance` and
  `look_read` use the same describers.

**The contract** (`app/describe.h`): a window type's main class, the window or the view it wraps, derives
from `ww::Describable` and implements `describeState()` and, when it holds text, `describeText()`. Window
classes whose sources the film engine compiles byte for byte (`windows/frame_animation_window`,
`frame_file_player_view`, `gradient`) are described from outside: `ww::addExternalDescriber` in
`wwdos_app.cpp`. Dialogs are described by kind (`ww::dialogState`).

**The audit**: `python3 tools/describe_audit.py` lists every `TWindow`/`TDialog` class compiled into
`wwdos` (from `app/CMakeLists.txt`) with how it is described (`self`, `view`, `external`, `dialog`) and exits
1 when one is `MISSING` or when a class a window-type matcher names is not covered. A new window class
fails it until it has a describer.

## Person events: the agent sees the human

Every act authored by the console is also a gesture with its own number `g`. Three ways to receive them:

    cmd:person_events since=12                       # what came after gesture 12, at once
    cmd:person_wait since=12 ms=10000 kinds=typed,opened   # the socket holds the reply until one arrives
    cmd:subscribe_events                              # pushed, as {"type":"event","event":"gesture","payload":{...}}

    {"last":14,"count":2,"timeout":false,"gestures":[
      {"g":13,"act":41,"ms":1825,"actor":"person","kind":"typed","what":"typed h","window":"w1",
       "type":"text_editor","update":false,"text":"h"},
      {"g":14,"act":41,"ms":1933,"actor":"person","kind":"typed","what":"typed hello","window":"w1",
       "type":"text_editor","update":true,"text":"hello"}]}

- `kind`: `typed`, `key`, `click`, `double-click`, `drag`, `scroll`, `command` (a menu or status-line
  command by name), `menu` (`what`: "opened menu Window"), `menu closed`, `opened`, `moved`, `resized`,
  `retitled`, `closed`. `type` is the window's type, kept for a window that has closed.
- `update: true`: the same act continued (more typing in one run, a drag, a run of moves); it carries the
  whole run so far. An unread update is replaced by the next one.
- `person_wait` answers within one pass of the event loop after the gesture (34 ms in the test) and with
  `timeout: true` and no gestures when `ms` pass (at most 60000).
- The stream is fed from the act log, so only acts by the console reach it. A resident's keys, its menu
  commands and the windows it opens are its own acts and never arrive as the person's.

## Residents in the machine

### One command table

The socket answers every registry command by its own name (`app/command_registry.cpp`), as
`exec_command name=`, the API server and MCP do. The residents' in-app hands use the socket's own
dispatcher in process, so they reach every command a socket caller can send.

### Hands inside the app

`resident::actAs(actor, command, args)` (`app/resident_hands.cpp`) runs a command as a resident (source
`resident`), logged in that actor's name. Resident input commands go through the resident queue, so the
actor's hand is seen moving before each act lands. A window a resident opens does not take the person's focus.

### Reflex scripts

A reflex script is a small text file in which residents answer the person's gestures. Load one at start with
`WWDOS_RESIDENTS=<file>` (whoever starts the app chooses it), or with `resident_reflexes load=<file>` (a rule:
refused from every socket caller). `resident_reflexes` reports what is loaded, what it bound and its last results.

    # comment
    start as wib -> notes: create_window type=text_editor x=2 y=22 w=56 h=14 title=residents
    on opened as wib: resident_key id={notes} text="wib: you opened a {type}\n"
    on typed type=text_editor as scramble: resident_key id={notes} text="scramble: {text}\n"

- `start` lines run once, when the app is idle after start; `-> name` binds the window the command opened.
- `on <kind> [type=<window type>] as <actor>: <command>` answers each person gesture of that kind. Typing is
  answered when the run pauses for 700 ms, with the whole run in `{text}`; drags and runs of moves are
  answered once.
- Placeholders: `{window}`, `{type}`, `{what}`, `{text}`, `{kind}`, and every bound name.
- Values with spaces are quoted; `\n`, `\t`, `\"` work inside quotes.
- Reflexes answer only the person, so a resident's own acts never set one off; gestures in the residents' own
  bound windows are not answered.

### Scramble's paws

Scramble's model reply may end with lines such as

    ACT: create_window type=text_editor x=2 y=20 w=30 h=6 title=paws
    ACT: resident_key id=new text=hello

The app takes them out of the reply (they are not shown) and runs each as actor `scramble` through the
resident path, with her hand. Allowed: `resident_key`, `resident_mouse`, `resident_press`, `resident_menu`,
`create_window`, `move_window`, `resize_window`, `close_window`, `window_title`, `describe`, `look_glance`,
`look_read`, the sound commands, and the terminal transport (`terminal_pause`, `terminal_resume`, `terminal_toggle`,
`terminal_state`, `terminal_release`, `cast_control`, `cast_state`; `docs/terminal-transport.md`). Anything else is refused and logged. `id=new` names the window her previous act opened. Her
prompt carries the desktop's own window list (from the describers) and the asker's stamp; an instruction
whose standing is `data` (read from a file) cannot make her act. Her model call runs with
`--system-prompt`, `--setting-sources ""`, `--tools ""` and `--strict-mcp-config`: it returns text and
cannot reach the person's machine.

### The Wib&Wob chat

The chat's agent has two tools (`app/llm/sdk_bridge/mcp_tools.js`): `tui_list_commands` and
`tui_menu_command` with `as: wib|wob`. They talk to the app's own socket (`app_socket.js`, path in
`WWDOS_SOCKET`, set by the app), as actor wib or wob with source `resident`; no API server needs to run.
The agent is confined (`confine.js`): only those tools, every built-in disallowed, a `canUseTool` that
refuses anything else, no settings sources, strict MCP, an empty temporary working directory.
`node app/llm/sdk_bridge/probe_confine.js` checks it with two haiku calls: confined, no tool is tried and
no file is made; with the bridge's old options the model ran Bash and Write and made two files.

Install once per checkout or worktree (the chat says so in one line when it is missing):

    npm ci --prefix app/llm/sdk_bridge

The chat's prompt file is not in this repo. Point at it with `WWDOS_PROMPT=<path to wibandwob.prompt.md>`;
without it the chat uses a short built-in prompt and greets with "Wib&Wob". `WWDOS_CHAT_MODEL` overrides
the chat's model (the self-test uses haiku).

## The Feed

Tools > Feed, or `cmd:open_feed [by=person|wib|wob|scramble|visitor|system] [x= y= w= h=]`.

    17:01:31 PER command New Test Pattern  [w2]
    17:01:31 PER opened test_pattern "" at 2,1 48x14  [w3]
    17:01:31 WIB typed "wib: you opened a test_pattern./"  [w1]
    17:01:33 SYS scramble's model call finished after 3.2 s
    17:01:40 VIS eri: resident_reflexes load=...  refused: only the console with the key turned ...

One line per act, newest at the bottom: time, tag, the actor's name when the tag does not say it, what was
done, the window, and the result when it is not plain success. A run of a resident's keys is one
`typed "..."` line (`/` stands for Enter).

| tag | colour | who |
|---|---|---|
| PER | white | the person at the console (keyboard, mouse, menus) |
| WIB | yellow | Wib |
| WOB | cyan | Wob |
| SCR | magenta | Scramble |
| VIS | green | a visitor: any other socket or API caller (its name follows the tag) |
| GST | dark green | a guest (another seat, the room chat) |
| API | brown | the API server's default name |
| RES | blue | another named resident |
| SYS | grey | the app itself: timers, start, model calls started and finished with their seconds |

Refused lines are red. The feed follows the log while it is scrolled to the bottom; scrolling back stops
following and End follows again. Keys `a` all, `p` person, `w` Wib, `o` Wob, `s` Scramble, `v` visitors,
`y` system. Reading the log is not an act, so the feed's own redraws never appear in it. `describe` gives
its lines as text.

## Self-test with real models

    WWDOS_SELFTEST_MODELS=1 python3 tests/resident/selftest_residents.py

Opt-in (it makes about six haiku calls). It starts its own desktop with the claude CLI on PATH and
`WWDOS_CHAT_MODEL=haiku`, sends Scramble and the Wib&Wob chat each "open a text editor and type hello in it"
and "which windows are open?" over the socket as a visitor, checks the desktop for an editor holding hello
owned by that resident and a reply that names a window that is on the desktop, opens the Feed and reads its
lines back. It prints PASS or FAIL per resident. Needs `npm ci --prefix app/llm/sdk_bridge`.

## Tests

`python3 tests/resident/test_resident_world.py [describe] [wheel] [gestures] [hands]`: its own desktops
(`tmux -L worldtest`, `tmux -L handstest`), no model; `hands` puts a stand-in claude CLI on PATH that answers
with canned text and records its arguments.

| group | what it shows |
|---|---|
| describe | the audit lists 45 window classes and none lacks a describer; 34 window types open and each says what it is by contract; the editor's text and cursor come from its model, also when the window is fully covered; window_text reads a paint canvas; a chat gives its messages; a dialog is described by its controls |
| wheel | the mouse wheel while a resident's hand and reading mark are on screen leaves the desktop running (on main it overflowed the stack); a click on the hand reaches the window under it |
| gestures | person_wait holds its reply and answers 34 ms after the person's key; typing on is an update; none of 12 resident acts reaches the stream; opening a menu, opening, dragging and closing a window are gestures in order, each an act of the person; subscribe_events pushes them |
| hands | a reflex script answers the person's open, typing and close with wib's, scramble's and wob's hands, leaves the person's focus alone and never sets itself off; loading a script from the socket is refused; Scramble's ACT lines type into a window and open an editor and type in it, her call has no tools, her prompt carries the desktop and the asker, quit_app is refused; the chat's socket client acts as wob; the Feed shows every tag, model calls with seconds, a refusal with its reason, follows new acts, filters by actor, and its redraws are not acts |

## What still runs outside the app

- **The chat's model** runs in a Node child process (`app/llm/sdk_bridge`), which needs `npm ci` in each
  checkout; its tools now reach the app's own socket, so the Python API server is no longer in the
  residents' path.
- **Scramble's model** runs as a `claude -p` child process per question.
- **The API server** (`tools/api_server`, port 8089) still serves HTTP callers and the show layer; it is a
  visitor to the app like any socket caller.
- **The stage language** keeps its own table (`app/stage/commands.def`) and the desktop app cannot play a stage.
  Folding it in means linking the stage driver into `wwdos` and generating its dispatch and the registry
  entries from one definition.
- **The menu bar** is built by hand in `wwdos_app.cpp`; residents reach its items by name (`resident_menu`),
  but menu items and registry commands are separate lists.

## What is weak

- **Socket-only commands are not in the registry list.** `get_state`, `create_window`, `move_window` and the
  other bare socket handlers answer by name but do not appear in `get_capabilities`; the chat's tool text
  names them by hand.
- **Attribution by comparison.** A window change made outside an event (a chat window's `select()` when a
  reply ends, a timer) is attributed to the author of the event being handled, or to the app when idle.
- **Owners are reported, not enforced.** Scramble's paws and the chat may move or close the person's windows.
- **Anyone who may talk may make Scramble or the chat act.** A visitor's or guest's instruction is stamped
  with its standing and her acts are logged in her name, but only `data` standing stops her acting.
- **Nothing is written to disk.** The act log, gestures and the feed live in memory (4000 acts).
- **Wib and Wob share a hand letter (W).** Hands are told apart by position only; the Feed tells them apart.
- **The chat still drops what the person types while it is thinking.** Queued input is not built.
- **Under heavy machine load** (load average above 16) a socket reply occasionally took more than 5 s, and
  the test client then read an empty reply; the same suites pass at normal load.
- **The demo's reflexes are scripted**, not chosen: a resident with a model does not yet decide which gesture
  to answer.
- **A resident waiting on a dialog** and a recovered crash are not yet system lines in the Feed.
