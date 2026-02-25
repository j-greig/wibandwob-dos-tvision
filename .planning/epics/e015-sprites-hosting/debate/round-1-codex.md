# Round 1 — Codex response

> NOTE: Codex rate-limited; this round written by Claude playing devil's advocate.
> Replace with real Codex output after 19:13 UTC if desired.

## NOT AGREED — shared tmux has a fatal identity flaw

Claude's proposal is seductive but breaks the core feature of RoomChatView.

### The identity problem is not a minor nit

RoomChatView's entire value is **per-participant identity**: adjective-animal names,
`(me)` labels, consistent colours. This requires each participant to have their
own PartyKit connection with their own `conn.id` → name mapping.

In the shared-tmux model there is **one wwdos process, one bridge, one PartyKit
connection**. From PartyKit's perspective there is one participant, not N. Every
browser user is the same person. The presence strip shows `1 in room`. The `(me)`
label and the self-colour are shared between all physical users. Chat messages
all come from the same sender name.

You haven't added multi-user — you've added multi-viewer of one user.

### Per-connection spawning is NOT complex

Claude calls it "per-user orchestration" making it sound heavy. It's not:

```bash
# ttyd with --once spawns a fresh process per connection
ttyd --port 8080 --writable --once \
  bash -c 'WIBWOB_INSTANCE=$$ ./build/app/wwdos'
```

`$$` = shell PID = guaranteed unique instance ID per connection. IPC socket path
uses the instance ID (`/tmp/wibwob_$$.sock`). Each connection gets:
- Own wwdos process (~15MB RAM)
- Own API server on a dynamic port (or skip it — RoomChatView doesn't need API for chat)
- Own bridge with own PartyKit conn.id → own adjective-animal name

Three lines of shell. "Complex" is the wrong word.

### The bridge is the real overhead, not the TUI

Each user needs their own bridge (WebSocket connection to PartyKit). The TUI
process is ~15MB. The bridge is a Python asyncio script — also ~20MB. Per-user
cost: ~35MB RAM. On a Sprite with 512MB+ RAM: ~10 concurrent users easily.

If the bridge is the concern, cut it: **connect browsers directly to PartyKit
WebSocket** for the web layer, and only use the bridge for the TUI. But that
means rewriting the web layer. Simpler to just spawn the bridge per connection.

### Revised proposal: ttyd `--once` + `$$` instance IDs

```
[Browser A]              [Browser B]
     │                        │
     └──────── ttyd ───────────┘
               --once --writable
               │          │
          wwdos(PID=101)  wwdos(PID=247)
          bridge→PartyKit  bridge→PartyKit
          [name: amber-gnu] [name: sage-jay]
               │                  │
               └──────────────────┘
                       PartyKit room
                       2 in room ✓
```

Each browser gets a real TUI with their own identity. PartyKit sees 2 participants.
Presence strip works. `(me)` label works. Colours work.

### What I'm less sure about

- Does `WIBWOB_INSTANCE=$$` work given the IPC socket naming convention? Need to
  verify the socket path formula in `api_ipc.cpp`
- Does ttyd `--once` restart cleanly when a user disconnects and reconnects?
- Sprite RAM ceiling — what's the actual RAM limit on a Sprites.dev microVM?
- The API server per user adds complexity — can RoomChatView work without it
  (bridge + TUI direct IPC only)?
