---
name: ww-room-chat
description: Launch and test the WibWob-DOS multi-instance PartyKit room chat. Spins up two TUI instances (wibwob1 + wibwob2), two API servers (8089 + 8090), two PartyKit bridges sharing one fresh room, and opens a RoomChatView on both. Use when you want to test room chat, check presence strip, verify bidirectional relay, inject test messages, or do a full E008 smoke test. Triggers on "room chat", "ww-room-chat", "test room chat", "start partykit", "wibwob1 wibwob2", "multi chat", "two instances".
---

# ww-room-chat

Launch two WibWob-DOS instances sharing a PartyKit room. Verify presence, bidirectional chat relay, adjective-animal names.

## Start

```bash
.claude/skills/ww-room-chat/scripts/start.sh            # fresh room (rchat-HHMMSS)
.claude/skills/ww-room-chat/scripts/start.sh rchat-myroom  # named room
```

## Stop

```bash
.claude/skills/ww-room-chat/scripts/stop.sh
```

## Attach to watch

```bash
tmux attach -t wibwob1   # TUI 1 — your adjective-animal, labelled (me)
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
for s in wibwob1 wibwob2; do
  echo "=== $s ==="
  tmux capture-pane -t $s -p | sed 's/\x1b\[[0-9;]*m//g' \
    | grep -E "╔|║.*•|║.*\]|║.*in room|╟"
done
```

### Inject a test message (direct IPC, bypasses PartyKit)
```bash
curl -s http://127.0.0.1:8089/menu/command -X POST \
  -H "Content-Type: application/json" \
  -d '{"command":"room_chat_receive","args":{"sender":"test-bot","text":"hello from agent","ts":"12:00"}}'

curl -s http://127.0.0.1:8090/menu/command -X POST \
  -H "Content-Type: application/json" \
  -d '{"command":"room_chat_receive","args":{"sender":"test-bot","text":"hello from agent","ts":"12:00"}}'
```

### Push a presence update
```bash
curl -s http://127.0.0.1:8089/menu/command -X POST \
  -H "Content-Type: application/json" \
  -d '{"command":"room_presence","args":{"participants":["dark-jay","grim-gnu","swift-fox"],"count":3}}'
```

### Watch bridge logs live
```bash
tail -f /tmp/b1.log   # bridge → instance 1
tail -f /tmp/b2.log   # bridge → instance 2
```

### Check API state
```bash
curl -s http://127.0.0.1:8089/state | python3 -m json.tool
curl -s http://127.0.0.1:8090/state | python3 -m json.tool
```

### Full agent smoke test sequence
1. Capture both TUIs — confirm `2 in room` + two names
2. Inject msg into TUI 1 from `"sender":"tester"`
3. Wait 1s, capture TUI 1 — confirm msg appears
4. Wait bridge poll (~500ms), capture TUI 2 — confirm relay arrived
5. Inject from TUI 2, confirm both see it
6. Report pass/fail

## Key details

| Thing | Value |
|-------|-------|
| TUI 1 session | `wibwob1` |
| TUI 2 session | `wibwob2` |
| API 1 | `http://127.0.0.1:8089` |
| API 2 | `http://127.0.0.1:8090` |
| Bridge logs | `/tmp/b1.log`, `/tmp/b2.log` |
| PartyKit server | `wibwob-rooms.j-greig.partykit.dev` |
| Room naming | `rchat-HHMMSS` — always fresh to avoid stale DO state |
| IPC commands | `room_chat_receive`, `room_presence`, `room_chat_pending` |
| Command IDs | `cmRoomChat=182`, `cmRoomChatReceive=183`, `cmRoomChatSend=184`, `cmRoomPresence=185` |
| Self colour | `TColorRGB(120,220,140)` — soft green, reserved for `(me)` entries |
