# Round 0 — Claude's opening position

## Recommended architecture: tmux-shared single TUI + ttyd

### One process, one Sprite, everyone shares it

```
[Browser A]   [Browser B]   [Browser C]
     │               │               │
     └───────────────┼───────────────┘
                     ▼
          ttyd :8080 (xterm.js frontend)
                     │ WebSocket → PTY
                     ▼
          tmux session "wibwob"
                     │
                     ▼
          wwdos TUI (one process)
          WIBWOB_INSTANCE=1
                     │
          ┌──────────┴──────────┐
          API :8089          bridge → PartyKit room
```

**ttyd behaviour:** by default ttyd serves ONE process to ALL connections — all
browsers share the same PTY output stream. Everyone sees the same TUI screen.
With tmux in between, all clients see the same tmux session. Multiple users can
type — first to hit Enter wins. Input races are part of the aesthetic.

### Why shared terminal wins here

1. **Zero dynamic instance allocation** — one `WIBWOB_INSTANCE`, one IPC socket,
   one bridge. No per-user setup, no race conditions on socket paths.

2. **Scales to N users for free** — adding a 10th viewer costs zero extra RAM.
   tmux handles N read/write clients natively.

3. **The chaos is the point** — WibWob-DOS is a symbient operating system.
   Multiple humans fighting over one input bar is surreal and on-brand. It's
   not a bug, it's the vibe.

4. **Sprite stays simple** — 3 services: wwdos (TTY session), API (Service),
   bridge (Service). No per-user orchestration.

5. **ttyd already does this** — `ttyd tmux attach -t wibwob` serves the session
   to every browser connection with zero extra code.

### Failure modes I see

- **Input contention**: two users typing simultaneously → garbled input bar.
  Mitigation: the TUI input bar is one line; it clears on Enter. Race window
  is small. Acceptable for low user counts (≤5 concurrent).

- **Screen size negotiation**: tmux picks the smallest attached client terminal
  size. If one browser window is tiny, everyone gets a tiny TUI.
  Mitigation: `tmux set -g window-size largest` — uses largest client instead.

- **No per-user identity**: the TUI sees one sender ("me" label). If two people
  type as "me" the chat history mixes them. Mitigation: bridge assigns
  adjective-animal names per PartyKit connection — the *PartyKit* side already
  has identity. Only the local TUI's "me" label is ambiguous.

### Sprites setup (minimal)

```bash
# One-time
apt install ttyd
git clone <repo> /home/sprite/app && cd /home/sprite/app
cmake -B build && cmake --build build --target wwdos
uv sync --project tools/api_server

# Services (auto-restart on wake)
sprite-env services create api --cmd uvicorn ...
sprite-env services create bridge --cmd uv run tools/room/partykit_bridge.py ...

# TTY session (persistent via tmux)
sprite exec -tty -- tmux new-session -s wibwob \
  "WIBWOB_INSTANCE=1 ./build/app/wwdos"

# ttyd on port 8080 (Sprite URL default)
sprite-env services create ttyd \
  --cmd ttyd --args "--port 8080 --writable tmux attach -t wibwob"

sprite url update --auth public
```

### What I'm least confident about

- Whether tvision's ncurses init works correctly inside ttyd's PTY on a
  headless Sprite (no $TERM issues? colour depth ok?)
- Whether `tmux set window-size largest` actually fixes the size problem
  or just shifts the problem to "largest client dominates"
- If Sprites routes HTTP to port 8080 specifically or "first open port" —
  needs a live test

## My verdict

Shared tmux session via ttyd is the right MVP. If input contention becomes
genuinely unpleasant in practice, we graduate to per-connection process
spawning (ttyd `--once` mode in a loop) as a follow-on. But start simple.
