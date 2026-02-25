---
id: E015
title: Sprites Hosting — Multi-User WibWob-DOS via xterm.js
status: not-started
issue: TBD
pr: —
depends_on: [E008]
---

# E015: Sprites Hosting — Multi-User WibWob-DOS via xterm.js

Host WibWob-DOS on a **single [sprites.dev](https://sprites.dev) microVM** so
any number of remote users can open the full TUI in their browser (xterm.js via
ttyd), each with their own identity in a shared PartyKit room. No local setup
required for remote users.

## TL;DR

One Sprite runs `ttyd`. Each browser connection forks a fresh `wwdos` + bridge
process with a unique `WIBWOB_INSTANCE=$$` (shell PID) → unique IPC socket →
own PartyKit connection → own adjective-animal name. All users land in the same
room. Presence strip shows real count. Identity works. ~35MB RAM per user.

---

## Architecture

```
                sprites.app HTTPS URL (public)
                        │
                ttyd :8080 — forks per connection
                        │
     ┌──────────────────┼──────────────────┐
  Browser A          Browser B          Browser C
  INST=101           INST=247           INST=389
  wwdos+bridge       wwdos+bridge       wwdos+bridge
  amber-gnu          sage-jay           swift-fox
     │                  │                  │
     └──────────────────┴──────────────────┘
                  PartyKit "rchat-live"
                     3 in room ✓
```

**Key**: ttyd default behaviour forks the command for each new browser
connection. No `--once` flag needed. Each fork gets `INST=$$` (shell PID) →
`/tmp/wibwob_$$.sock` → fully isolated wwdos instance.

---

## Design decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Process model | Per-connection fork (ttyd default) | Each user = own identity in PartyKit |
| Instance ID | `WIBWOB_INSTANCE=$$` (shell PID) | Guaranteed unique; confirmed in `wwdos_app.cpp:997` |
| API server | Not deployed per user | Not in the human TUI→PartyKit path |
| Room name | Fixed env var `rchat-live` | Zero coordination; V2 can add lobby |
| Sprite count | 1 | ttyd + N forked children all on one microVM |
| HTTP port | 8080 (Sprite URL default) | ttyd listens on 8080 |

---

## Features

| # | Title | Status | Detail |
|---|-------|--------|--------|
| F01 | Sprite setup script | not-started | `scripts/sprite-setup.sh` — deps, git clone, cmake build, uv sync |
| F02 | User session wrapper | not-started | `scripts/sprite-user-session.sh` — INST=$$, bridge wait loop, TUI foreground |
| F03 | ttyd Service registration | not-started | `sprite-env services create tui-host` — auto-restarts on Sprite wake |
| F04 | Bridge exit fix | not-started | `partykit_bridge.py` — socket-gone = exit, not retry (zombie prevention) |
| F05 | Public URL + room env | not-started | `sprite url update --auth public`, `WIBWOB_PARTYKIT_ROOM=rchat-live` |
| F06 | GitHub auto-redeploy | not-started | GH Action on push to main → `sprite exec git pull && cmake --build` |

---

## Key files (to create/modify)

| File | Change |
|------|--------|
| `scripts/sprite-setup.sh` | New — one-time Sprite provisioning |
| `scripts/sprite-user-session.sh` | New — per-connection ttyd wrapper |
| `.github/workflows/sprite-redeploy.yml` | New — auto-redeploy on push |
| `tools/room/partykit_bridge.py` | Fix — exit on socket-gone, not retry |

---

## Acceptance criteria

| Check | Test |
|-------|------|
| Two browsers open Sprite URL, both see TUI | Visual |
| Presence strip shows correct count | `2 in room` with 2 browsers |
| Each browser has distinct adjective-animal name | Check sidebar |
| Message sent from browser A appears in browser B | Chat relay |
| Closing browser tab → bridge process exits (no zombie) | `sprite exec -- ps aux | grep bridge` |
| Sprite wakes and ttyd is ready after idle period | Open URL after 10min idle |

---

## Sprites reference (for implementers)

```bash
sprite create wibwob-dos          # create the Sprite
sprite use wibwob-dos             # set as active
sprite exec -- <cmd>              # run one-off command
sprite exec -tty -- <cmd>        # interactive TTY
sprite console                    # interactive shell
sprite proxy 8080                 # forward Sprite :8080 → localhost:8080
sprite url update --auth public  # make URL public
sprite url                        # show URL
sprite exec -- free -m            # check RAM
sprite-env services create ...   # register auto-restart daemon
sprite checkpoint create          # snapshot filesystem
```

URL format: `https://wibwob-dos-<org>.sprites.app`

---

## Open questions (pre-implementation)

| # | Question | Answer via |
|---|----------|-----------|
| 1 | Sprite RAM ceiling | `sprite exec -- free -m` |
| 2 | tvision ncurses init in headless ttyd PTY | Smoke test |
| 3 | Does Sprite URL route to port 8080? | `sprite proxy 8080` then curl test |
| 4 | `sprite-env services` exact syntax | Check installed CLI |

---

## Debate record

Architecture settled over 4 rounds in `debate/`. Key decisions:
- Round 0→1: shared-tmux rejected (one PartyKit identity, presence broken)
- Round 1→2: per-connection fork accepted; API server dropped from user path
- Round 2→3: bridge zombie risk identified (socket retry on exit)
- Round 4: agreed architecture + bridge fix scoped

## Status

Status: not-started — create GitHub issue before starting F01
GitHub issue: TBD
PR: —

## References

- Sprites docs: https://docs.sprites.dev
- ttyd: https://github.com/tsl0922/ttyd
- E008 (PartyKit room chat, done): `.planning/epics/e008-multiplayer-partykit/`
- Debate transcript: `.planning/epics/e015-sprites-hosting/debate/`
