# Resident input

A resident (an agent living in the desktop) operates WIBWOB-DOS beside the person at the keyboard. It presses any
key, clicks, drags and scrolls at cell coordinates, reads what dialog or menu is in front, presses dialog controls
by label and chooses menu items by name. Its input has a queue of its own and can carry an address (a window id),
so it reaches that window without moving the person's focus, and the person keeps typing while it works.

Code: `app/resident_input.cpp` (joined to the event loop in `TWwdosApp::getEvent` and `TWwdosApp::putEvent`).
Tests: `tests/resident/test_resident_input.py`. No Turbo Vision change is needed; `vendor/tvision` is untouched.

## Reaching the commands

Every command is a registry command, so it is reachable in four ways:

| route | example |
|---|---|
| IPC socket | `cmd:resident_key id=w3 keys=Ctrl+S` |
| registry over IPC | `cmd:exec_command name=resident_key id=w3 keys=Ctrl+S` |
| API server | `POST /menu/command {"command":"resident_key","args":{"id":"w3","keys":"Ctrl+S"}}` |
| MCP | `tui_menu_command` with the same command and args (`app/llm/sdk_bridge/mcp_tools.js`); `tui_list_commands` lists them |

Values are percent-encoded on the IPC line (a space is `%20`).

## Commands

Coordinates are desktop cells, the same as `get_state` (x from the left edge, y 0 is the row under the menu bar).
`screen=1` takes screen cells instead (row 0 is the menu bar); with `id`, `local=1` takes cells from the window's
top-left corner.

### resident_key

Keys, in order, to window `id`, or with no `id` to wherever the person's keys go (the focused window, or the
dialog or menu in front). `keys` is a space-separated list; `text` types each character (newline is Enter, tab is Tab).
Both may be given; `keys` go first.

    cmd:resident_key id=w3 keys=Home%20Shift+End%20Del%20Ctrl+Right%20F2
    cmd:resident_key id=w3 text=hello%20there

Key names: a single character, `Enter Esc Tab Backspace (BS) Space Up Down Left Right Home End PgUp PgDn Ins Del F1`
to `F12`, each with `Ctrl+`, `Alt+`, `Shift+` in any combination, or a raw Turbo Vision key code (`0x1c0d`). An unknown
name refuses the whole call: `err unknown key: Hyper+Q`.

The reply names the queued items: `{"ok":true,"first":41,"last":45,"count":5,"queued":5}`.

### resident_mouse

`action=click|double|down|up|move|drag|scroll` at `x,y`; `drag` needs `x2,y2` (and `steps`), `scroll` takes `dir=up|down|left|right`
and `count`; `button=left|right|middle`, `mods=ctrl,shift,alt`.

    cmd:resident_mouse id=w2 action=drag x=12 y=1 x2=40 y2=10     # drag w2 by its title bar
    cmd:resident_mouse id=w2 action=scroll dir=up count=5 x=48 y=6

With `id` the point must be inside that window (`err point outside window w2`). A drag is queued as one down, the
moves and one up, and is delivered in one go; a `down` with no `up` behind it gets an up added when a view tracks the
mouse, so a view never waits on the resident's button.

### resident_status

The queue, the delivery log since a sequence number, the person's state and the windows the resident opened.

    cmd:resident_status since=40

    {"queued":0,"delivered":45,"refused":1,"next_seq":46,"waiting":"","resident_button_down":false,
     "person":{"window":"w1","buttons":0,"idle_ms":5210,"focus":"w1"},"share_ms":1500,"owned":["w4"],
     "log":[{"seq":41,"what":"key Home","target":"w3","result":"delivered"}, ...]}

Results: `delivered`, `delivered (not handled)` (no view took the key), `pressed`, `set`, `refused: Sharing violation`,
`refused: window gone`, `refused: control disabled`. An item is logged when its delivery starts, so a dialog it opens is
already in front when its entry appears.

### resident_manners

    cmd:resident_manners share_ms=1500 wait_on_button=1
    cmd:resident_manners clear=1            # drop everything queued (logged as refused: cleared)

### resident_front

What is in front: the modal dialog or open menu, else the focused window; each control with its kind, label, value
and whether it is enabled; how to accept and cancel; other dialogs on the desktop; who owns each.

    cmd:resident_front

    {"modal":true,"menu_open":false,"focused":"w1",
     "front":{"kind":"dialog","id":"","title":"Save Workspace As","owner":"resident","modal":true,
       "controls":[{"kind":"input","label":"Name:","value":"","enabled":true},
                   {"kind":"button","label":"OK","command":10,"default":true,"enabled":true},
                   {"kind":"button","label":"Cancel","command":11,"default":false,"enabled":true}],
       "accept":{"press":"OK","key":"Enter"},"cancel":{"press":"Cancel","key":"Esc"}},
     "dialogs":[]}

Kinds: `button`, `input`, `checkboxes`, `radio`, `cluster`, `list` (first 50 items), `text`, and `view` with its C++ class
for anything else. A label is the text of the `TLabel` linked to the control, or a button's title, without hot-key
tildes. An open menu reports `kind: menu`, its items and the highlighted one. Dialogs opened with `messageBox` or
`inputBox` sit in the application group, not the desktop, and have no window id (`"id":""`).

### resident_press

Press a control by its label in window `id`, or in the dialog in front (else the front-most dialog on the desktop).
`item=` picks an entry of a list or a check box or radio group; `value=` sets an input line. A label also matches an
item's text directly. Matching ignores case, tildes, a trailing `:` or `...`; an exact match wins, else a unique prefix.

    cmd:resident_press label=Name value=evening-set
    cmd:resident_press label=OK

A button press is `TButton::press()`; the command it puts goes back to that dialog (see Delivery).

### resident_menu

With no arguments, the menu bar as a tree of names (`command`, `enabled`, `key`). `choose=` queues one item's command,
by path (`File>Save Workspace As`) or by a unique item name. `id=` offers the command to that window first, then to the
application.

    cmd:resident_menu choose=Help>About%20WIBWOBWORLD

### window_text

The lines a text editor window holds, so a caller can check what its keys produced.

    cmd:window_text id=w3     ->  {"id":"w3","type":"text_editor","lines":["hello there"]}

## Delivery

```
  person: terminal ──▶ TProgram::getEvent ──┐
                                            ├─▶ TWwdosApp::getEvent ──▶ the loop that is running
  resident: IPC ──▶ resident queue ─────────┘        (main loop, a dialog's execView, a mouse drag)
```

- **One queue, in order.** Every key, pointer event, press and menu command is an item with a sequence number. One
  item is delivered per pass of the event loop while the person is idle; `TEventQueue::wakeUp()` keeps the loop from
  sleeping its 20 ms while items wait. Items for one window keep their order; an item for a window that is busy
  (see below) waits without holding up items for other windows. `putEvent` holds one event (a second replaces the
  first), which is why `inject_command` loses commands in a burst; the resident path does not use it.
- **Unaddressed** items go into the running loop exactly as the person's input does (status-line hot keys apply), so a
  key goes to the focused window or the open dialog or menu.
- **Addressed** items are handed to the window's own `handleEvent`. Inside the window, keys reach the window's own
  current view, as they would if it were focused. For a click the window's `sfSelected|sfActive` bits (and its frame's
  `sfActive`) are set for the length of the dispatch only, so `TView::handleEvent` does not select (raise and focus) it
  and the frame's close and zoom icons work.
- **The person's focus is restored.** If `deskTop->current` changed during a resident dispatch (a window it opened, a view
  that called `select()`), the person's window is made current again with `setCurrent`; z-order is left as it is, so a
  window the resident opens is drawn on top with an inactive frame.
- **Puts come back to the window.** A command or key a view puts during a resident dispatch (a frame's `cmClose`, a
  button's command, a dialog's `cmDefault` on Enter) is queued first for the same window instead of going to the person's
  focused window. A command the window leaves unhandled is offered to the application.
- **Mouse tracking.** Between the resident's down and up its own pointer events come first and nothing is read from
  the terminal, so a view's `mouseEvent()` loop sees only the resident's drag.
- **Dialogs the resident opens.** A dialog that resident input opens (`execView` inside the resident's dispatch) is the
  resident's: `resident_front` reports `owner: resident`, its loop takes resident items, and the person's keys go to their own
  window (the one that was current when the resident's input started, or the last one they clicked) and their clicks
  to the window under the pointer, without raising it. When the dialog closes, the window they last clicked is current. Windows that appear during a resident dispatch are listed in `resident_status.owned`.

Containment of resident dialogs, and where it stops:

| contained | not contained |
|---|---|
| the person's typing and clicks into desktop windows continue | the person cannot use the menu bar or status line while a resident dialog runs; status-line hot keys become commands inside the resident's dialog and are dropped |
| the resident's dialog takes only resident input | the person cannot raise a window by clicking it; the click is delivered and their keys follow it, but the window stays where it is in the stack until the dialog closes |
| `resident_press` answers it by label | the window whose input opened the dialog is busy until the dialog closes: resident items for it wait, the person's input to it waits and is replayed after |
| | the dialog is still modal in Turbo Vision's sense: the code that opened it runs inside the resident's dispatch until it returns, and a second resident dialog nests inside the first |
| | a dialog the person opens is the person's, and a resident dialog the person's input reaches is not made non-modal |

## Manners

- **The person's button.** While the person's mouse button is down (a drag, a held click), resident items wait; they
  are delivered in order after the release. `resident_status.waiting` says so.
- **Sharing violation.** Resident input to the window the person is acting in is refused with
  `err Sharing violation: the person is acting in w3`, at the call and again at delivery (`refused: Sharing violation` in
  the log). The person is acting in a window while their button is down in it and for `share_ms` (1500 ms) after their
  last key, click or scroll there. A dialog the person's input opened counts as theirs over the same time.
  Unaddressed keys land in the person's window, so they are refused while the person types. A menu command with no
  `id` is the application's and is not checked.

## Focus table

Which window types test for focus, from reading each view's code and from a probe that sent `Up`, `x` and `Tab` to
each type, addressed, while the person's window kept the focus (`handled` = a view took the key). In every case the
person's focus stayed where it was. `Tab` is handled everywhere by `TWindow` (next view inside the window).

| window type | keys addressed while not focused | tests for focus in the code |
|---|---|---|
| text_editor | Up, x handled | cursor drawn only when focused (draw) |
| terminal | Up, x handled | cursor shown on focus (tvterm `setState`) |
| wibwob (chat) | Up, x handled | prompt blink and input colour need focus (draw); sending a message and the end of a reply call the window's `select()`, which raises and focuses the chat window: during a resident dispatch the focus is restored, but the end-of-reply `select()` runs later and takes the person's focus |
| room_chat | x handled (Up unbound) | cursor drawn when focused |
| browser | Up handled (x unbound) | none |
| paint | Up handled (x unbound) | cursor cell drawn when focused; a click selects the canvas inside the window (no focus change outside it) |
| micropolis_ascii, rogue, deep_signal, quadra | Up handled; quadra also x | none |
| app_launcher, gallery | Up handled | gallery's file list selects itself on click, inside its window; launching puts a command (routed to the launcher, then the application) |
| tr808 | x handled (Up unbound) | none; step edits put events (routed back to the window) |
| score, monster_verse, generative_lab | x handled | none |
| snake | Up not handled while the game is idle | none |
| test_pattern, gradient, verse, mycelium, orbit, torus, cube, life, blocks, ascii, animated_gradient, monster_cam, contour_map, monster_portal | no key bindings (same for the person) | none |
| frame_player, text_view, figlet_text | not probed (need a path or text to open) | figlet_text: Edit menu font and text commands act on `deskTop->current` |
| backrooms_tv | not probed: opening one stops the app, on the unmodified build too | |

Stock Turbo Vision parts that test focus or selection:

| part | test | effect on resident input |
|---|---|---|
| `TView::handleEvent` | a click on an unselected selectable view calls `select()` | bypassed: the window is marked selected for the click |
| `TFrame` | close and zoom icons and the resize corner need `sfActive` | the frame is marked active for the click; title-bar drag needs nothing |
| `TButton` | Space presses only when focused | use `resident_press`, Enter (default button) or Alt+hot key |
| `TCluster` | arrow keys and Space only when focused | use `resident_press item=` |
| `TInputLine` | keys only when selected in its dialog | works: selection is per dialog, not per desktop |
| `TListViewer`, `TLabel`, `THistory` | colours follow selection and focus | drawing only |
| `TWindow` `cmClose`/`cmZoom` | put by the frame; focused events go to `deskTop->current` | puts inside a resident dispatch go back to the window |

Application commands that act on `deskTop->current` (the person's window) even when chosen by the resident with
`id=`: Window > Send to back (`cmSendToBack`), the Edit menu's FIGlet font and text commands, the animated score's
background palette, and the Wib&Wob chat commands that look for the focused chat window
(`findTargetWibWobWindow`).

## Tests

`python3 tests/resident/test_resident_input.py [s1] [s2] [duet] [s3]` starts its own desktop in a private tmux server
(`tmux -L residenttest`) with its own socket, and stops it after. `WWDOS_BIN` chooses the binary (default
`build-resident/app/wwdos`). The person types through tmux one key per write; Turbo Vision treats three or more
printable keys arriving together as a paste (`kbPaste`), and the text editor view drops pasted text.

| group | what it shows |
|---|---|
| s1 | 200 keys in one call and 200 keys in 200 calls arrive in order, none lost; named keys and modifiers act; an addressed drag moves a window by (20,20) without raising it or moving focus; 40 wheel steps move a scroll thumb |
| s2 | resident keys wait while the person's button is down and arrive after release; a Sharing violation for keys and clicks to the window the person typed in, accepted for another window at the same moment and for the same window after share_ms |
| duet | the person types 264 characters into their window through tmux while the resident types into another with Backspace, Home, Ctrl+Right and End; both texts arrive intact and the focus never moves. On the unmodified app it fails: the resident's commands are unknown and nothing reaches its window |
| s3 | five menu commands in one batch all run (five `inject_command` run once); a resident-opened About dialog is reported with its text, button and accept, the person keeps typing into their window while it is open and their keys follow their click to another window, OK by label closes it and the clicked window is current; the person's open menu and dialog are reported as theirs; resident keys reach another window while the person's dialog is modal; Save Workspace As: the field is set by its label and Cancel by label closes it |

## Not done

- **Who did what.** The log knows the resident's acts; the app does not record authorship of changes (item 6).
- **A visible hand.** Resident input moves no cursor on screen; the show layer's cursors are separate.
- **Dialogs as non-modal windows.** A dialog opened by resident input is still run by `execView` (see the containment
  table). Making it a plain window needs each call site to take its result later instead of from `execView`.
- **The person's dialogs.** A dialog the person opens still stops the person; the resident is not stopped by it.
- **Menus by keyboard.** An open menu is reported, but `resident_menu choose` sends the command directly; it does not walk
  the open menu.
- **Seats.** One person and one resident; there is no second resident queue and no per-seat colour.
- **Receipts that carry the change.** A log entry says delivered or refused, not what changed on screen.
- **Non-ASCII keys** arrive as text only (`keyCode` 0); views that read `charCode` do not see them.
