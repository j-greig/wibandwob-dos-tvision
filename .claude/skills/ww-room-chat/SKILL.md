---
name: ww-room-chat
description: Launch and test the WibWob-DOS multi-instance PartyKit room chat. Spins up two TUI instances (wibwob + wibwob2), two API servers (8089 + 8090), two PartyKit bridges sharing one fresh room, and opens a RoomChatView on both. Use when you want to test room chat, check presence strip, verify bidirectional relay, inject test messages, or do a full E008 smoke test. Triggers on "room chat", "ww-room-chat", "test room chat", "start partykit", "wibwob1 wibwob2", "multi chat", "two instances".
---

# ww-room-chat

Launch two WibWob-DOS instances sharing a PartyKit room. Verify presence, bidirectional chat relay, adjective-animal names.

## Start (fresh session)

```bash
cd /Users/james/Repos/wibandwob-dos

# Kill any existing sessions
tmux kill-server 2>/dev/null; sleep 0.3
rm -f /tmp/wibwob_*.sock /tmp/b1.log /tmp/b2.log

# Build if needed
cmake --build ./build --target test_pattern 2>&1 | tail -3

# Fresh room name — avoids stale Durable Object state
ROOM="rchat-$(date +%H%M%S)"
echo "Room: $ROOM"

# TUI instances
tmux new-session -d -s wibwob  "WIBWOB_INSTANCE=1 ./build/app/test_pattern 2>/tmp/wibwob_1_debug.log"
tmux new-session -d -s wibwob2 "WIBWOB_INSTANCE=2 ./build/app/test_pattern 2>/tmp/wibwob_2_debug.log"

# Wait for IPC sockets
until [[ -S /tmp/wibwob_1.sock && -S /tmp/wibwob_2.sock ]]; do sleep 0.4; done

# API servers
tmux new-session -d -s api1 "WIBWOB_INSTANCE=1 ./tools/api_server/venv/bin/uvicorn tools.api_server.main:app --host 127.0.0.1 --port 8089 2>&1"
tmux new-session -d -s api2 "WIBWOB_INSTANCE=2 ./tools/api_server/venv/bin/uvicorn tools.api_server.main:app --host 127.0.0.1 --port 8090 2>&1"

# Wait for APIs
until curl -sf http://127.0.0.1:8089/health &>/dev/null && curl -sf http://127.0.0.1:8090/health &>/dev/null; do sleep 0.5; done

# PartyKit bridges — same room, different instances
tmux new-session -d -s bridge1 "WIBWOB_INSTANCE=1 \
  WIBWOB_PARTYKIT_URL=https://wibwob-rooms.j-greig.partykit.dev \
  WIBWOB_PARTYKIT_ROOM=$ROOM \
  uv run tools/room/partykit_bridge.py 2>&1 | tee /tmp/b1.log"

tmux new-session -d -s bridge2 "WIBWOB_INSTANCE=2 \
  WIBWOB_PARTYKIT_URL=https://wibwob-rooms.j-greig.partykit.dev \
  WIBWOB_PARTYKIT_ROOM=$ROOM \
  uv run tools/room/partykit_bridge.py 2>&1 | tee /tmp/b2.log"

sleep 3

# Open RoomChatView on both
curl -s http://127.0.0.1:8089/windows -X POST -H "Content-Type: application/json" -d '{"type":"room_chat"}'
curl -s http://127.0.0.1:8090/windows -X POST -H "Content-Type: application/json" -d '{"type":"room_chat"}'

sleep 5
echo "Ready."
```

## Attach to watch

```bash
tmux attach -t wibwob    # TUI 1 — your adjective-animal, labelled (me)
tmux attach -t wibwob2   # TUI 2 — the other participant
# Ctrl-B D to detach without killing
```

## Expected initial state

Both TUIs should show:
- Title bar: `Room Chat`
- Sidebar: two `• adjective-animal` entries, one labelled `(me)`
- Footer: `2 in room`
- Welcome: `[HH:MM] system — Room chat connected. Say hello!`

## What the agent can do once running

### Capture TUI state
```bash
# Read both screens (strip ANSI, show only chat/presence lines)
for s in wibwob wibwob2; do
  echo "=== $s ==="
  tmux capture-pane -t $s -p | sed 's/\x1b\[[0-9;]*m//g' \
    | grep -E "╔|║.*•|║.*\]|║.*in room|╟"
done
```

### Inject a test message (bypasses PartyKit — direct IPC)
```bash
# Inject into TUI 1 as if received from remote
curl -s http://127.0.0.1:8089/menu/command -X POST \
  -H "Content-Type: application/json" \
  -d '{"command":"room_chat_receive","args":{"sender":"test-bot","text":"hello from agent","ts":"12:00"}}'

# Inject into TUI 2
curl -s http://127.0.0.1:8090/menu/command -X POST \
  -H "Content-Type: application/json" \
  -d '{"command":"room_chat_receive","args":{"sender":"test-bot","text":"hello from agent","ts":"12:00"}}'
```

### Check presence strip
```bash
# Push a presence update to TUI 1 (3 participants)
curl -s http://127.0.0.1:8089/menu/command -X POST \
  -H "Content-Type: application/json" \
  -d '{"command":"room_presence","args":{"participants":["dark-jay","grim-gnu","swift-fox"],"count":3}}'
```

### Watch bridge logs live
```bash
tail -f /tmp/b1.log   # bridge 1 → instance 1
tail -f /tmp/b2.log   # bridge 2 → instance 2
```

### Check API health + state
```bash
curl -s http://127.0.0.1:8089/state | python3 -m json.tool
curl -s http://127.0.0.1:8090/state | python3 -m json.tool
```

### Full smoke test sequence (agent-driven)
1. Capture both TUIs — confirm `2 in room` + two names
2. Inject msg into TUI 1 from `"sender":"tester"`
3. Wait 1s, capture TUI 1 — confirm msg appears
4. Wait bridge poll (~500ms), capture TUI 2 — confirm relay arrived
5. Inject msg into TUI 2 from `"sender":"other-tester"`
6. Wait 1s, capture TUI 2 + TUI 1 — confirm both see it
7. Report pass/fail

## Stop

```bash
tmux kill-server
```

## Key details

| Thing | Value |
|-------|-------|
| TUI 1 session | `wibwob` |
| TUI 2 session | `wibwob2` |
| API 1 | `http://127.0.0.1:8089` |
| API 2 | `http://127.0.0.1:8090` |
| Bridge logs | `/tmp/b1.log`, `/tmp/b2.log` |
| PartyKit server | `wibwob-rooms.j-greig.partykit.dev` |
| Room naming | `rchat-HHMMSS` — always fresh to avoid stale DO state |
| IPC commands | `room_chat_receive`, `room_presence`, `room_chat_pending` |
| Command IDs | `cmRoomChat=182`, `cmRoomChatReceive=183`, `cmRoomChatSend=184`, `cmRoomPresence=185` |
| Self colour | `TColorRGB(120,220,140)` — soft green, reserved for `(me)` entries |
