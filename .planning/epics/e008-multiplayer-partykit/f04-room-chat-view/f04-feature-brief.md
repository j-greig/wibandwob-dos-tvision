---
id: E008-F04
title: RoomChatView — multi-user room chat
status: in-progress
github_issue: 99
pr: —
parent_epic: E008
---

# E008 F04: RoomChatView

## What this is

A dedicated chat widget inside WibWob-DOS for people sharing a PartyKit room.

Two humans teleport into the same WibWob-DOS instance via a browser URL.
They can see each other's windows open and move in real time.
They need a place to talk. That's this.

Later: one or two AI agents (Wib, Wob) can join the same chat as named participants.
No special code needed for that — agents just send messages with a different sender label.

---

## What it looks like

```
┌─ Room: wibwob-shared ──────────────────────────────────────────┐
│ ● human:1      │ [14:12] human:1  hello                       │
│ ● human:2      │ [14:12] human:2  hi! can you see my windows? │
│                │ [14:13] human:1  yes — paint window appeared  │
│ 2 in room      ├────────────────────────────────────────────── │
│                │ > _                                           │
└────────────────┴──────────────────────────────────────────────┘
```

- **Left strip** (~16 cols): who's in the room, presence dot, count
- **Right body**: scrolling message log, newest at bottom
- **Bottom input**: single line, Enter to send
- **Sender colours**: each participant gets a consistent colour (hash of sender string → TV palette index 1–6)

---

## Participants

| Sender string | Who |
|---|---|
| `human:1` | visitor on instance 1 |
| `human:2` | visitor on instance 2 |
| `wib` | Wib AI agent (future) |
| `wob` | Wob AI agent (future) |
| `system` | room join/leave notices |

When agents join later: the bridge sends `chat_msg` with `sender: "wib"`.
The view renders it in a different colour. No code change needed.

---

## How it works

```
human types → RoomChatView input → IPC event → bridge → PartyKit chat_msg
                                                              ↓
                                                    broadcast to all others
                                                              ↓
                                              bridge on other instance
                                                              ↓
                                              IPC room_chat_receive → RoomChatView
```

The PartyKit server (`partykit/src/server.ts`) already handles `chat_msg` broadcast
and `presence` join/leave events. No server changes needed.

---

## What needs building

### C++: `app/room_chat_view.h` + `app/room_chat_view.cpp`

New TView window with three subviews:
- `TRoomParticipantStrip` — draws participant list (left panel)
- `TRoomMessageView` — scrolling message log (right panel, extends TScroller)
- `TRoomInputLine` — single-line input at bottom

Window class: `TRoomChatWindow`
Factory function: `createRoomChatWindow(const TRect& bounds)`
Command constant: `cmRoomChat = 182`

IPC commands handled by the window:
- `cmRoomChatReceive (183)` — append `{sender, text, ts}` to message log
- `cmRoomPresence (185)` — update participant strip with `{participants: [...]}`

### Bridge: `tools/room/partykit_bridge.py`

Route incoming PartyKit messages:
- `chat_msg` → IPC `room_chat_receive {sender, text, ts}`
- `presence` → IPC `room_presence {participants}`

Keep old routing behind `legacy_scramble_chat` flag (default False) so existing tests pass.

### Python API: `tools/api_server/`

- `models.py` — add `room_chat = "room_chat"` to `WindowType` enum
- `schemas.py` — add `"room_chat"` to `WindowCreate` Literal
- `controller.py` — add `room_chat` case in `create_window()`

### C++ registry: `app/command_registry.cpp`

Add capabilities:
- `open_room_chat` — opens a RoomChatView window
- `room_chat_receive` — delivers an incoming message `{sender, text, ts}`
- `room_presence` — updates participant list `{participants}`

---

## Acceptance criteria

| # | Check | Test |
|---|---|---|
| AC-1 | `POST /windows {"type":"room_chat"}` opens window | curl → 200, screenshot shows window |
| AC-2 | `room_chat_receive` IPC appends message with sender label + colour | send IPC directly, screenshot |
| AC-3 | `room_presence` IPC updates participant strip | send IPC with 2 participants, screenshot |
| AC-4 | Typing + Enter sends `chat_msg` via bridge to PartyKit | bridge log shows outbound message |
| AC-5 | Message from instance 1 appears in instance 2's RoomChatView | live 2-instance test |

---

## Stories

| # | Title | Status |
|---|---|---|
| S01 | C++ RoomChatView skeleton (window, panels, draw) | [x] |
| S02 | IPC: room_chat_receive + room_presence | [x] |
| S03 | Bridge: route chat_msg + presence to RoomChatView | [ ] |
| S04 | Input → outbound relay via bridge | [ ] |
| S05 | Integration test + update e008-demo.md | [ ] |

---

## Out of scope

- AI response generation (agents just send messages like humans)
- Cursor overlay / ghost cursors
- Visitor identity / auth
- Message persistence across disconnect
