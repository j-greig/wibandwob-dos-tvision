---
id: E015
title: Sprites Hosting — Remote Multi-User WibWob-DOS
status: not-started
issue: TBD
pr: —
depends_on: [E008]
---

# E015: Sprites Hosting — Remote Multi-User WibWob-DOS

Host WibWob-DOS on [sprites.dev](https://sprites.dev) so remote users can join a shared PartyKit room via browser, with no local setup. Each user gets their own Sprite (isolated TUI instance). All Sprites share one PartyKit room via the bridge.

## TL;DR

Sprites = persistent Linux microVMs with a public HTTPS URL, auto-sleep when idle, instant wake on request. Plan: each user's TUI runs inside their own Sprite, exposed via `ttyd` (WebSocket → PTY → browser terminal). All bridges point to the same PartyKit room → N users in one room.

---

## What are Sprites?

Source: https://docs.sprites.dev

| Feature | Detail |
|---------|--------|
| OS | Ubuntu 24.04 LTS |
| Storage | 100 GB persistent ext4 (survives hibernation) |
| RAM | Does **not** persist — processes restart on wake |
| URL | `https://<name>.sprites.app` — auto-wakes Sprite on request |
| HTTP port | Routes to port 8080 by default |
| Billing | Per-second compute; free when idle |
| Preinstalled | Node, Python, Go, Git, Claude CLI, Codex, `uv`, common dev tools |
| Isolation | Hardware-level microVM (not just containers) |

### Key CLI commands

```bash
sprite create <name>              # create a new Sprite
sprite use <name>                 # set active Sprite (skip -s flag)
sprite exec -- <cmd>              # run one-off command
sprite exec -tty -- <cmd>        # TTY session (interactive)
sprite console                    # interactive shell
sprite sessions list              # list running sessions
sprite sessions attach <id>       # reattach detached session
sprite proxy 8089                 # forward Sprite port 8089 → localhost:8089
sprite url                        # show HTTPS URL
sprite url update --auth public   # make URL publicly accessible
sprite checkpoint create          # snapshot filesystem
sprite destroy                    # irreversible — deletes everything
```

### Services — the key primitive for persistent daemons

RAM doesn't persist through hibernation. **Services** auto-restart when the Sprite wakes:

```bash
sprite-env services create my-server --cmd uvicorn --args "tools.api_server.main:app --port 8080"
```

Use Services for: API server, PartyKit bridge, ttyd. Use TTY sessions for: TUI (user-facing, detachable).

### Sessions — detachable TTY

```bash
sprite exec -tty -- tmux new-session ./build/app/wwdos
# Ctrl+\ to detach
sprite sessions attach <id>       # reattach
```

---

## Architecture

```
[User A browser]                        [User B browser]
       │ HTTPS                                 │ HTTPS
       ▼                                       ▼
[Sprite A: wibwob-alice.sprites.app]   [Sprite B: wibwob-bob.sprites.app]
  ttyd :8080 → PTY → wwdos TUI           ttyd :8080 → PTY → wwdos TUI
  API server :8089                        API server :8089
  PartyKit bridge                         PartyKit bridge
       │ WebSocket                               │ WebSocket
       └──────────────┐  ┌────────────────────┘
                      ▼  ▼
              [PartyKit: wibwob-rooms.j-greig.partykit.dev]
                      room: rchat-xyz
```

Each user browses to their Sprite's URL → sees the full DOS TUI in their browser terminal. Messages typed in one TUI appear in all others via PartyKit within ~500ms.

---

## Recommended approach: ttyd-per-Sprite

**Why not shared Sprite?** tvision/ncurses = one terminal per TUI instance. Two users sharing one PTY = input conflicts. Isolation wins.

**Why not web frontend?** Full rewrite, loses DOS aesthetic entirely. ttyd preserves everything for zero C++ changes.

**Why ttyd?** It's exactly this use case: wraps any terminal app in a WebSocket/browser terminal. `apt install ttyd`. Routes HTTP → PTY. Browser gets xterm.js. Zero changes to the TUI binary.

---

## Features

| # | Title | Status | Detail |
|---|-------|--------|--------|
| F01 | Sprite setup script | not-started | One-time provisioning: deps, build, uv, git clone |
| F02 | Services wiring | not-started | API server + bridge as auto-restart Services |
| F03 | ttyd integration | not-started | TUI exposed via ttyd on port 8080, Sprite URL → public |
| F04 | Room coordination | not-started | Mechanism for users to discover/join a shared room name |
| F05 | GitHub → auto-redeploy | not-started | GH Action triggers `sprite exec git pull && rebuild` on push |
| F06 | ww-room-chat skill update | not-started | Add remote Sprite launch path alongside local start.sh |

---

## Key Files (to create)

| File | Role |
|------|------|
| `scripts/sprite-setup.sh` | One-time Sprite provisioning (deps + build + services) |
| `scripts/sprite-start.sh` | Start TUI session + services on a Sprite |
| `scripts/sprite-stop.sh` | Stop services + TUI session |
| `.github/workflows/sprite-redeploy.yml` | Auto-redeploy on push to main |

---

## Deployment plan (step-by-step)

### Step 1 — Create and provision a Sprite

```bash
sprite create wibwob-alice
sprite use wibwob-alice
sprite exec -- apt-get install -y ttyd cmake git build-essential
sprite exec -- git clone https://github.com/j-greig/wibandwob-dos /home/sprite/app
sprite exec -- bash -c "cd /home/sprite/app && cmake -B build && cmake --build build --target wwdos"
sprite exec -- bash -c "cd /home/sprite/app && uv sync --project tools/api_server"
```

### Step 2 — Register Services (auto-restart on wake)

```bash
sprite exec -- sprite-env services create api \
  --cmd /home/sprite/app/tools/api_server/venv/bin/uvicorn \
  --args "tools.api_server.main:app --host 0.0.0.0 --port 8089" \
  --workdir /home/sprite/app --env "WIBWOB_INSTANCE=1"

sprite exec -- sprite-env services create bridge \
  --cmd uv \
  --args "run tools/room/partykit_bridge.py" \
  --workdir /home/sprite/app \
  --env "WIBWOB_INSTANCE=1,WIBWOB_PARTYKIT_URL=https://wibwob-rooms.j-greig.partykit.dev,WIBWOB_PARTYKIT_ROOM=rchat-live"
```

### Step 3 — Expose TUI via ttyd on port 8080

```bash
# ttyd on port 8080 (Sprite's default HTTP port)
# -W = writable (users can type), -t = title, --once = one session
sprite exec -tty -- ttyd --port 8080 --writable \
  bash -c "WIBWOB_INSTANCE=1 /home/sprite/app/build/app/wwdos"
```

### Step 4 — Make URL public

```bash
sprite url update --auth public
sprite url   # → https://wibwob-alice.sprites.app
```

User visits that URL → full WibWob-DOS TUI in browser.

### Step 5 — GitHub auto-redeploy

```yaml
# .github/workflows/sprite-redeploy.yml
on:
  push:
    branches: [main]
    paths: [app/**, tools/**, partykit/**]
jobs:
  redeploy:
    runs-on: ubuntu-latest
    steps:
      - name: Rebuild on Sprite
        env:
          SPRITE_TOKEN: ${{ secrets.SPRITE_TOKEN }}
        run: |
          sprite exec -s wibwob-alice -- bash -c \
            "cd /home/sprite/app && git pull && cmake --build build --target wwdos"
```

---

## Open questions

| # | Question | Notes |
|---|----------|-------|
| 1 | Does ttyd work with tvision's ncurses init in a headless PTY? | ttyd sends `TIOCSWINSZ` from browser — needs live test |
| 2 | Sprites HTTP URL → port 8080 confirmed? | Docs say "first HTTP port opened" — need to verify ttyd on 8080 gets routed |
| 3 | `sprite-env services` exact syntax | Docs show it but needs verification against installed CLI version |
| 4 | Room name coordination | Fixed env var (simplest) vs. lobby room vs. URL param |
| 5 | Multi-user Sprite naming convention | `wibwob-<username>` or `wibwob-<role>` (alice/bob/wibwob) |
| 6 | GitHub integration specifics | User says "Sprites linked to the symbient's GitHub repo" — what does the Sprites GitHub app actually do? |
| 7 | `WIBWOB_INSTANCE` per Sprite | Each Sprite = instance 1; no shared IPC socket conflicts |

---

## References

- Sprites overview: https://docs.sprites.dev
- CLI reference: https://docs.sprites.dev/cli/commands
- Working with Sprites: https://docs.sprites.dev/working-with-sprites
- ttyd: https://github.com/tsl0922/ttyd
- E008 (PartyKit room chat): `.planning/epics/e008-multiplayer-partykit/`
- ww-room-chat skill: `.claude/skills/ww-room-chat/SKILL.md`

## Status

Status: not-started
GitHub issue: TBD — create before starting F01
PR: —
