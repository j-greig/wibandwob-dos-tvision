# E015 Architecture Debate Protocol

## Purpose
Claude and Codex debate the best architecture for single-Sprite multi-user
WibWob-DOS with xterm.js TUI delivery. Loop runs until both sides mark a
round "AGREED" or explicitly converge on a solution.

## Files
```
debate/
  DEBATE-PROTOCOL.md     ← this file (rules + context, read every round)
  round-0-claude.md      ← Claude's opening position
  round-1-codex.md       ← Codex round 1 response
  round-2-claude.md      ← Claude round 2 rebuttal/update
  round-3-codex.md       ← Codex round 3
  ...
  round-N-agreed.md      ← Final agreed architecture (written by whoever converges first)
```

## Rules for each Codex round

1. **Read ALL previous rounds** before writing your response (they are your memory)
2. **Challenge assumptions** — don't just agree; find the weakest point in the previous round
3. **Be concrete** — if you disagree with an approach, name a specific failure mode or cost
4. **Propose alternatives** — don't just critique, offer a better path
5. **Mark convergence** — if you agree with the previous round, start your response with `AGREED:` and write the final brief instead of a rebuttal
6. **Max 500 words per round** — stay sharp, no padding
7. **Update the epic brief** — the final agreed round should produce a revised `e015-epic-brief.md`

## Context for every round (read this every time)

### The goal
WibWob-DOS is a C++ tvision/ncurses TUI (DOS aesthetic). We want:
- Remote users to see + use the TUI via a browser (xterm.js / ttyd)
- Multiple users in one PartyKit room (chat + presence already works)
- Hosted on a **single Sprites.dev microVM** — not one per user
- Minimal infrastructure cost, maximum DOS vibe

### What already exists and works
- `./build/app/wwdos` — C++ TUI binary (ncurses, requires PTY)
- `tools/api_server/` — FastAPI per instance (port 8089)
- `tools/room/partykit_bridge.py` — Python WS bridge → PartyKit
- `partykit/src/server.ts` — Durable Object at `wibwob-rooms.j-greig.partykit.dev`
- RoomChatView in TUI — bidirectional chat, adjective-animal names, presence strip
- `WIBWOB_INSTANCE=N` env var — differentiates multiple local instances via IPC socket path

### Sprites.dev key facts
- Ubuntu 24.04 LTS microVM, 100GB persistent storage
- URL: `https://<name>.sprites.app` → routes to port 8080 by default
- **RAM does NOT persist** — processes need Services (auto-restart on wake) or TTY sessions
- `sprite-env services create` — registers a daemon that restarts on wake
- `sprite exec -tty` — detachable TTY session (Ctrl+\ to detach)
- Per-second billing, free when idle
- Preinstalled: Python, Node, uv, git, common dev tools
- ttyd: `apt install ttyd` — serves any terminal app over WebSocket/HTTP (xterm.js frontend built-in)

### The core tension
**Isolation vs simplicity:**
- Isolated (each user own TUI process) = N processes on one Sprite, needs dynamic instance IDs, N bridges to PartyKit
- Shared (one TUI, tmux multiwriter) = one process, one bridge, but shared input = chaotic
- Hybrid = one canonical TUI (Wib&Wob's view), web users get a lightweight chat widget not the full TUI

### Open questions to resolve
1. Does ttyd spawn one process per browser connection, or serve one shared process?
2. Can tvision/ncurses init correctly inside a ttyd PTY (no $DISPLAY, headless)?
3. Is shared-terminal chaos acceptable for the WibWob aesthetic (might actually be fun)?
4. How many concurrent wwdos processes can a Sprite realistically handle?
5. Dynamic WIBWOB_INSTANCE allocation — file lock? env injection at ttyd spawn time?
