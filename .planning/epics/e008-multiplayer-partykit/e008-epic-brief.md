---
id: E008
title: Multiplayer Teleport Rooms (PartyKit)
status: in-progress
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
| F01 | PartyKit Server | ✅ done | TS Durable Object on `wibwob-rooms.j-greig.partykit.dev`. 12 tests. |
| F02 | WebSocket Bridge | ✅ done | Python sidecar (`partykit_bridge.py`). Polls IPC 500ms, diffs, pushes/receives deltas. |
| F03 | State Diffing | ✅ done | `state_diff.py` — `compute_delta`/`apply_delta`, add/remove/update buckets. 26 tests. |
| F04 | Chat Relay | ✅ done | Bidirectional Scramble ↔ PartyKit. Seq-based dedup. `chat_receive` IPC command. 11 tests. |
| F05 | Cursor Overlay | ⬜ stretch | Inject JS into ttyd for ghost cursors. Parked — ttyd page structure not stable. |
| F06 | Room Config | ✅ done | `multiplayer: true` YAML fields. Orchestrator passes env vars. Validation. |

Feature briefs: `f0X-*/f0X-feature-brief.md`

## Key Files

| File | Role |
|------|------|
| `partykit/src/server.ts` | PartyKit Durable Object server |
| `tools/room/partykit_bridge.py` | WS bridge sidecar |
| `tools/room/state_diff.py` | Delta computation |
| `tools/room/orchestrator.py` | Spawns bridge when `multiplayer: true` |
| `tools/room/room_config.py` | Parses multiplayer YAML fields |
| `app/api_ipc.cpp` | IPC protocol incl. `chat_receive` command |

## Definition of Done (MVP)

| Check | Code | Live test |
|-------|------|-----------|
| Two browsers connect to same room | ✅ F01+F02+F06 | Pending |
| Window open/close/move syncs ≤500ms | ✅ F02+F03 | Pending |
| Chat messages sync between instances | ✅ F04 | Pending |
| Presence shows who's connected | ✅ F01 | Pending |
| Single-player rooms still work | ✅ 138/138 tests | Pass |

## Remaining

1. **Live integration** — needs two-browser test against deployed PartyKit
2. **F05 cursor overlay** — formally park or attempt in future pass
3. **PR** — open when live tests pass

## Parking Lot

- **C++ WebSocket client (ixwebsocket)**: F02 pivot. brew unavailable, libwebsockets event-loop conflict. Python bridge achieves same result.
- **F05 Cursor Overlay**: ttyd page structure not a stable API. xterm.js canvas coordinate mapping fiddly. Park unless ttyd adds `--inject-js` flag.

## Status

Status: in-progress
GitHub issue: #65
PR: —

## References

- [PartyKit server API](https://docs.partykit.io/reference/partyserver-api/)
- [E007 epic](../e007-browser-hosted-deployment/e007-epic-brief.md) (room infra dependency)
