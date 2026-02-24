---
id: E008-F04
title: RoomChatView — multi-user room chat
status: done
github_issue: 99
pr: —
parent_epic: E008
---

# E008 F04: RoomChatView

## TL;DR

Two humans share a PartyKit room via a WibWob-DOS bridge and can chat in a
dedicated TUI widget. Participants get memorable adjective-animal names.
AI agents can join as named participants with zero extra code.

---

## What shipped

### C++ (`app/room_chat_view.h` / `.cpp`)
- `TRoomParticipantStrip` — 18-col left panel, shows `• eerie-gnu (me)` / `• fast-tern`, count
- `TRoomMessageView` — TScroller right panel, colour-coded sender headers
- `TRoomInputLine` — bottom input, Enter to send
- `TRoomChatWindow` — owns all three, static global `g_roomChatWindow`
- `normaliseMsgTs()` — converts epoch-ms ts to `HH:MM` at the IPC boundary
- `senderColor()` — `"me"` and `"name (me)"` → fixed soft-green; others → hash palette

### IPC / API
- Commands: `cmRoomChat=182`, `cmRoomChatReceive=183`, `cmRoomChatSend=184`, `cmRoomPresence=185`
- `TTestPatternApp::handleEvent` catches `cmRoomPresence` + `cmRoomChatReceive` at app level
  (delivers even when window is not the focused command target)
- `api_get_state` emits `"type"` field via `dynamic_cast` so state diff can identify window types

### State sync hardening (`tools/room/state_diff.py`)
- `room_chat` added to `_INTERNAL_TYPES` — never synced as a layout window across instances
- `apply_delta_to_ipc` skips create for internal types
- Bridge `state_sync` handler uses `windows_from_state()` for filtering

### Bridge (`tools/room/partykit_bridge.py`)
- Routes `chat_msg` → `room_chat_receive` IPC, `presence` → `room_presence` IPC
- `_normalise_ts()` converts epoch-ms ts to `HH:MM` before IPC delivery
- `_name_for_conn()` maps PartyKit UUID → `adjective-animal` (deterministic MD5 hash)
- `_display_name()` appends `" (me)"` for self entry in strip
- `push_chat` sends `ts: HH:MM` so server doesn't need `Date.now()` fallback
- `presence: sync` handler stores `_self_conn_id` + populates `_participants` from full list
- Outbound sender = `_name_for_conn(_self_conn_id)` — matches strip label
- Poll heartbeat re-pushes participants every 5s so strip catches up if window opened late

### PartyKit server (`partykit/src/server.ts`) — deployed to `wibwob-rooms.j-greig.partykit.dev`
- `onConnect`: sends joiner a `presence: sync` with `connections: [...]` + `self: conn.id`
  so the joining bridge immediately knows all participants and its own ID
- `chat_msg` relay: bridge now sends `ts` so server `Date.now()` fallback rarely fires

---

## Acceptance criteria

| # | Check | Result |
|---|---|---|
| AC-1 | `POST /windows {"type":"room_chat"}` opens window | ✅ confirmed |
| AC-2 | `room_chat_receive` IPC appends message with sender label + colour | ✅ confirmed |
| AC-3 | `room_presence` IPC updates participant strip | ✅ `2 in room`, names shown |
| AC-4 | Typing + Enter sends `chat_msg` via bridge to PartyKit | ✅ bridge log shows outbound |
| AC-5 | Message from instance 1 appears in instance 2's RoomChatView | ✅ live 2-instance test |

---

## Stories

| # | Title | Status |
|---|---|---|
| S01 | C++ RoomChatView skeleton (window, panels, draw) | [x] |
| S02 | IPC: room_chat_receive + room_presence | [x] |
| S03 | Bridge: route chat_msg + presence to RoomChatView | [x] |
| S04 | Input → outbound relay via bridge | [x] |
| S05 | Integration test + 2-instance live demo | [x] |

---

## What was harder than expected

- **State pollution**: `room_chat` not in `_INTERNAL_TYPES` → bridge applied remote
  state_sync which closed the window and cleared `g_roomChatWindow`
- **Presence timing**: PartyKit notifies existing connections of the join but NOT
  the joiner — fixed by adding `presence: sync` + `self` field in server `onConnect`
- **Event routing**: `cmRoomPresence` dropped when window not focused — fixed by
  catching it at `TTestPatternApp::handleEvent` level
- **Epoch-ms timestamps**: `push_chat` sent no ts → server filled `Date.now()` →
  raw numbers rendered in TUI — fixed at bridge + IPC boundary

---

## Out of scope / follow-ons

- AI agent join (agents just send `chat_msg` with different sender — zero extra code)
- Cursor overlay / ghost cursors
- Visitor identity / auth beyond animal name
- Message persistence across disconnect
- Widen word lists for more name variety
