---
id: E008
title: Multiplayer Teleport Rooms (PartyKit)
status: done
issue: 65
pr: ~
depends_on: [E007]
---

# E008: Multiplayer Teleport Rooms (PartyKit)

Two users share a WibWob-DOS room via PartyKit. Side-by-side TUI instances with state mirror, Python WebSocket bridge, shared chat. PartyKit Durable Objects coordinate rooms.

## Directive

**Park, don't block.** MVP is two humans in synced rooms with chat. Everything else is gravy.

## Architecture

```
[Browser A]                      [Browser B]
   │ ttyd                           │ ttyd
   ▼                                ▼
[WibWob A] ←→ [bridge A]    [bridge B] ←→ [WibWob B]
   (IPC)         │  (WS)       (WS)  │       (IPC)
                 └──────┐  ┌────────┘
                        ▼  ▼
                  [PartyKit Server]
                  Durable Object / room
                  ├── canonical state
                  ├── chat messages
                  └── presence
```

1. User A acts → bridge polls IPC, diffs, pushes delta to PartyKit
2. PartyKit stores + broadcasts to other connections
3. Bridge B receives delta → applies via IPC commands
4. Screens converge within ~500ms (poll interval)

## Design Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Sync model | Side-by-side instances | Avoids cursor fights of shared terminal |
| State authority | PartyKit Durable Object | Canonical state at edge, server-side conflict resolution |
| Transport | Python sidecar bridge (not C++ WS) | ixwebsocket unavailable via brew; libwebsockets event-loop conflicts with TV. Python bridge = zero C++ changes, full testability |
| Chat | Relay via PartyKit | Scramble messages broadcast to all; remote messages injected via IPC without triggering AI |
| Sync scope | Window layout + chat | Not terminal character buffer or art engine params |

## Features

| # | Title | Status | Detail |
|---|-------|--------|--------|
| F01 | PartyKit Server | ✅ done | TS Durable Object on `wibwob-rooms.j-greig.partykit.dev`. `onConnect` sends joiner `presence:sync` with full connection list + `self` ID. |
| F02 | WebSocket Bridge | ✅ done | Python sidecar (`partykit_bridge.py`). Adjective-animal names, ts normalisation, `_INTERNAL_TYPES` guard, poll-heartbeat presence re-push. |
| F03 | State Diffing | ✅ done | `state_diff.py` — `compute_delta`/`apply_delta`. `room_chat` in `_INTERNAL_TYPES` so it never syncs as a layout window. |
| F04 | RoomChatView | ✅ done | Dedicated TUI chat widget (`TRoomChatWindow`). 18-col participant strip with adjective-animal names + `(me)` label. Bidirectional relay. Consistent self-colour. #99 |
| F05 | Cursor Overlay | ⬜ parked | Inject JS into ttyd for ghost cursors. ttyd page structure not stable. |
| F06 | Room Config | ✅ done | `multiplayer: true` YAML fields. Orchestrator passes env vars. Validation. |

Feature briefs: `f0X-*/f0X-feature-brief.md`

## Key Files

| File | Role |
|------|------|
| `partykit/src/server.ts` | PartyKit Durable Object — presence sync, chat relay, state delta |
| `tools/room/partykit_bridge.py` | WS bridge sidecar — adjective-animal names, ts normalisation, presence heartbeat |
| `tools/room/state_diff.py` | Delta compute + apply — `_INTERNAL_TYPES` guards room_chat/wibwob/scramble |
| `tools/room/orchestrator.py` | Spawns bridge when `multiplayer: true` |
| `tools/room/room_config.py` | Parses multiplayer YAML fields |
| `app/room_chat_view.h/.cpp` | TRoomChatWindow — strip, scroller, input, colour, ts normalisation |
| `app/window_type_registry.cpp` | `room_chat` spawn + match registration |
| `app/api_ipc.cpp` | IPC protocol — room_chat_receive, room_presence, room_chat_pending |

## Definition of Done (MVP)

| Check | Code | Live test |
|-------|------|-----------|
| Two instances connect to same room | ✅ F01+F02+F06 | ✅ `grim-gnu` + `dark-jay` 2-in-room |
| Chat messages sync between instances | ✅ F04 | ✅ bidirectional relay confirmed |
| Presence shows who's connected | ✅ F01+F04 | ✅ both sides show `2 in room` immediately |
| Names consistent across strip + chat | ✅ F04 | ✅ adjective-animal deterministic both sides |
| Window open/close/move syncs | ✅ F02+F03 | ⏳ follow-on — E008 next story |
| Single-player rooms still work | ✅ all tests | ✅ |

## Follow-ons (not blocking done)

1. **Window mirroring** — user 1 opens a window → appears on user 2. State delta machinery exists (F02+F03); needs `room_chat` excluded from sync and window-open events wired into bridge outbound. Good Codex task.
2. **F05 cursor overlay** — parked. Revisit if ttyd gets stable JS injection.
3. **Close GitHub issue #65** when PR merged.

## Parking Lot

- **C++ WebSocket client (ixwebsocket)**: F02 pivot. brew unavailable, libwebsockets event-loop conflict. Python bridge achieves same result.
- **F05 Cursor Overlay**: ttyd page structure not a stable API. xterm.js canvas coordinate mapping fiddly. Park unless ttyd adds `--inject-js` flag.

## Status

Status: done — MVP shipped. F01–F04+F06 complete, merged to main. F05 parked. Window mirroring is the natural next story.
GitHub issue: #65
PR: merged to main (feat/e008-f04-room-chat-view)

## References

- [PartyKit server API](https://docs.partykit.io/reference/partyserver-api/)
- [E007 epic](../e007-browser-hosted-deployment/e007-epic-brief.md) (room infra dependency)
