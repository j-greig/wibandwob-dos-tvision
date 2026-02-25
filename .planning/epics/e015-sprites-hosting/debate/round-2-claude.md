# Round 2 — Claude concedes and refines

## Conceded: the identity problem is fatal for shared-tmux

Round 1 is correct. One PartyKit connection = one participant = presence strip
shows `1 in room` no matter how many browsers are watching. That kills the core
feature. Shared-tmux is fine for a spectator mode, not for multi-user chat.

## Accepted: per-connection spawning is 3 lines of shell

And I can verify it works: `app/wwdos_app.cpp:997-1000` confirms:

```cpp
const char* inst = std::getenv("WIBWOB_INSTANCE");
// ...
sockPath = std::string("/tmp/wibwob_") + inst + ".sock";
```

So `WIBWOB_INSTANCE=$$` → `/tmp/wibwob_12345.sock`. Each ttyd-spawned shell
gets a unique PID → unique socket → fully isolated wwdos instance. Zero
collision risk.

## New simplification: drop the API server from the per-user path

The API server is for programmatic access (agents, testing, CI). Human users
interacting via the TUI PTY don't touch it. The RoomChatView ↔ PartyKit path
is: **keyboard → PTY → wwdos → IPC socket → bridge → PartyKit**. No API server
in that chain.

Per-user stack is actually just:
- `wwdos` process (~15MB RAM)
- `partykit_bridge.py` (~20MB RAM, needs the IPC socket to poll)

**~35MB per user.** A Sprite with 512MB handles ~14 concurrent users.
A Sprite with 2GB handles 50+. Very manageable.

## Revised architecture

```
[Browser A]         [Browser B]         [Browser C]
     │                   │                   │
     └─────────── ttyd :8080 ────────────────┘
                  --once --writable
                  spawns per connection:
                  bash -c 'INST=$$ WIBWOB_INSTANCE=$INST
                           ./wwdos &
                           sleep 1  # wait for IPC socket
                           WIBWOB_INSTANCE=$INST uv run bridge.py'
                  │                   │                   │
             wwdos(101)          wwdos(247)          wwdos(389)
             bridge(101)         bridge(247)         bridge(389)
             [amber-gnu]         [sage-jay]          [swift-fox]
                  │                   │                   │
                  └───────────────────┴───────────────────┘
                              PartyKit room
                              3 in room ✓
```

## The wrapper script (per ttyd connection)

```bash
#!/usr/bin/env bash
# scripts/sprite-user-session.sh
# Called by ttyd for each browser connection
INST=$$
export WIBWOB_INSTANCE=$INST
export WIBWOB_PARTYKIT_URL=https://wibwob-rooms.j-greig.partykit.dev
export WIBWOB_PARTYKIT_ROOM=${WIBWOB_PARTYKIT_ROOM:-rchat-live}

# Start bridge in background (waits for socket)
(until [ -S /tmp/wibwob_${INST}.sock ]; do sleep 0.2; done
 uv run /home/sprite/app/tools/room/partykit_bridge.py) &

# Start TUI in foreground (bridge waits for it)
exec /home/sprite/app/build/app/wwdos
# When user closes browser → TUI exits → bridge gets SIGPIPE → dies cleanly
```

ttyd command:
```bash
ttyd --port 8080 --writable --once \
  /home/sprite/app/scripts/sprite-user-session.sh
```

## Remaining open questions

1. **ttyd `--once` + reconnect**: when user closes/reopens browser, does ttyd
   spin up a fresh process? Yes — `--once` means one session per ttyd invocation,
   but Sprite's Service restarts ttyd after each exit. Or use ttyd without
   `--once` — it spawns a new child per connection natively (this is its default!).
   Actually: ttyd default IS per-connection spawning. `--once` means exit after
   first connection disconnects. Without `--once` = N concurrent connections each
   get their own process. **No flag needed.**

2. **Sprite RAM**: docs don't state a number. Need `sprite exec -- free -m` on
   a real Sprite to know the ceiling.

3. **Bridge cleanup on disconnect**: when the PTY closes (browser tab closed),
   wwdos exits → bridge loop gets socket error → should exit cleanly. Need to
   verify bridge exits on IPC socket gone rather than hanging.

4. **Room name distribution**: how does a new user know which room to join?
   Simplest: fixed `WIBWOB_PARTYKIT_ROOM=rchat-live` env var baked into the
   Sprite. All users always join `rchat-live`. No coordination needed.

## What I think is now settled

- ✅ Per-connection process spawning (ttyd default behaviour, no flags needed)
- ✅ `WIBWOB_INSTANCE=$$` for socket isolation (confirmed in source)  
- ✅ No API server per user (not in the human TUI path)
- ✅ Single Sprite hosts N users (~35MB each)
- ✅ Fixed room name in Sprite env var (simplest coordination)
- ⏳ Bridge cleanup on disconnect (needs live test)
- ⏳ Sprite RAM ceiling (needs `sprite exec -- free -m`)
- ⏳ tvision ncurses init in ttyd PTY (needs live test)
