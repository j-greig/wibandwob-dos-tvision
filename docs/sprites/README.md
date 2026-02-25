# WibWob-DOS on Sprites.dev

> Host the full TUI in a browser via xterm.js — multiple users, one Sprite, zero local setup for participants.

## TL;DR

One [Sprites.dev](https://sprites.dev) microVM runs `ttyd`. Each browser
connection gets its own isolated `wwdos` process + PartyKit bridge with a
unique identity. All users land in the same room. Presence strip shows the
real count. ~35 MB RAM per user.

```
[Browser A]   [Browser B]   [Browser C]
      └──────── ttyd :8080 ────────────┘
                forks per connection
        wwdos+bridge  wwdos+bridge  wwdos+bridge
        amber-gnu     sage-jay      swift-fox
              └──────── PartyKit room ──────────┘
                         3 in room ✓
```

---

## Prerequisites

- `sprite` CLI installed and authed (`sprite org auth`)
- Repo cloned locally (for running setup scripts)
- PartyKit server deployed (`wibwob-rooms.j-greig.partykit.dev`)

---

## Quick start (5 commands)

```bash
# 1. Create the Sprite
sprite create wibwob-dos
sprite use wibwob-dos

# 2. Upload and run setup (provisions deps, builds binary, registers Services)
sprite exec -- bash -c "$(cat scripts/sprite-setup.sh)"

# 3. Open to the world
sprite url update --auth public
sprite url        # → https://wibwob-dos-<org>.sprites.app

# 4. Send the URL to a friend. You both open it. Chat.
```

---

## How it works

### Per-connection isolation (`WIBWOB_INSTANCE=$$`)

ttyd forks `scripts/sprite-user-session.sh` for each new browser connection.
The script sets `WIBWOB_INSTANCE=$$` (the shell PID — guaranteed unique), which
drives the IPC socket path:

```
/tmp/wibwob_<PID>.sock
```

Each user gets:
- Their own `wwdos` process (own TUI state, own input)
- Their own `partykit_bridge.py` (own WebSocket to PartyKit → own `conn.id` → own adjective-animal name)
- Their own identity in the presence strip

### Bridge lifecycle (F04 fix)

When a user closes their browser tab:
1. PTY closes → `wwdos` exits → IPC socket file deleted
2. Bridge detects socket gone (via `TUIExited` exception) → exits cleanly
3. No zombie processes accumulate on the Sprite

### Sprite Services

`ttyd` is registered as a Sprite Service — it auto-restarts whenever the
Sprite wakes from hibernation. RAM doesn't persist through hibernation, but
the filesystem (binary, scripts, env) does.

---

## Build variants (CMake presets)

```bash
# Local dev — Debug, fast rebuild, ./build/
cmake --preset dev && cmake --build --preset dev

# Sprites production — Release, stripped (Linux), ./build-sprites/
cmake --preset sprites && cmake --build --preset sprites

# Release without strip — works on macOS + Linux, ./build-release/
cmake --preset release && cmake --build --preset release
```

See [`CMakePresets.json`](../../CMakePresets.json) at repo root. The `sprites`
preset is Linux-only (the `-s` strip flag). On macOS, use `release`.

`sprite-user-session.sh` prefers `build-sprites/app/wwdos` and falls back to
`build/app/wwdos` if the sprites build isn't present.

---

## Maintenance

### Redeploy after a code push

```bash
sprite exec -- bash -c "cd /home/sprite/app && git pull --ff-only && cmake --preset sprites && cmake --build --preset sprites"
```

Or automate via GitHub Actions — see [`../../.github/workflows/sprite-redeploy.yml`](../../.github/workflows/sprite-redeploy.yml) (F06, not yet implemented).

### Check RAM headroom

```bash
sprite exec -- free -m
# Each user ≈ 35 MB (wwdos ~15 MB + bridge ~20 MB)
```

### Watch bridge logs for a session

```bash
# On the Sprite — list active sessions
sprite exec -- ls /tmp/wibwob_bridge_*.log

# Tail a specific session
sprite exec -- tail -f /tmp/wibwob_bridge_<PID>.log
```

### Check for zombie bridges

```bash
sprite exec -- ps aux | grep partykit_bridge
```

### Restart ttyd Service

```bash
sprite exec -- sprite-env services restart tui-host
```

### Take a checkpoint before risky changes

```bash
sprite checkpoint create
# Roll back with:
sprite restore
```

---

## Changing the room name

Room name is baked into the Sprite via `WIBWOB_PARTYKIT_ROOM` env var set at
Service registration time. To change it:

```bash
sprite exec -- sprite-env services update tui-host \
  --env "WIBWOB_PARTYKIT_ROOM=rchat-newroom,WIBWOB_PARTYKIT_URL=https://wibwob-rooms.j-greig.partykit.dev,APP=/home/sprite/app"
sprite exec -- sprite-env services restart tui-host
```

---

## Local testing (no Sprite needed)

```bash
# F04 bridge exit fix — Python unit tests
cd tools/room && uv run test_bridge_exit.py

# Session script mechanics — shell integration tests
bash tools/room/test_sprite_session.sh

# ttyd locally (macOS — ttyd is installed)
APP=$(pwd) WIBWOB_PARTYKIT_ROOM=rchat-test \
  ttyd --port 7799 --writable scripts/sprite-user-session.sh
# → open http://localhost:7799 in a browser
```

---

## Key files

| File | Purpose |
|------|---------|
| `scripts/sprite-setup.sh` | One-time Sprite provisioning (deps → build → Services) |
| `scripts/sprite-user-session.sh` | Per-connection wrapper (INST=$$, bridge wait, TUI exec) |
| `tools/room/partykit_bridge.py` | Bridge — includes F04 TUIExited exit fix |
| `tools/room/test_bridge_exit.py` | 5 Python tests for F04 fix |
| `tools/room/test_sprite_session.sh` | 8 shell tests for session mechanics |
| `CMakePresets.json` | dev / sprites / release build presets |
| `.planning/epics/e015-sprites-hosting/` | Epic brief + 4-round architecture debate |

---

## Architecture decisions

See the full debate record:
[`.planning/epics/e015-sprites-hosting/debate/`](../../.planning/epics/e015-sprites-hosting/debate/)

Key choices:

| Decision | Choice | Why |
|----------|--------|-----|
| Process model | Per-connection fork (ttyd default) | Each user = own PartyKit identity |
| Instance ID | `WIBWOB_INSTANCE=$$` | Shell PID, unique, confirmed in `wwdos_app.cpp:997` |
| API server | Not in per-user path | Humans use PTY, not API |
| Room name | Fixed env var `rchat-live` | Zero coordination; lobby is V2 |
| Sprite count | 1 | ttyd + N forked children, ~35 MB/user |
| HTTP port | 8080 | Sprite URL default |

---

## Open questions (pre-deploy)

| # | Question | How to answer |
|---|----------|--------------|
| 1 | Sprite RAM ceiling | `sprite exec -- free -m` |
| 2 | tvision ncurses in ttyd PTY | `sprite console` → `ttyd --port 8080 --writable ./build-sprites/app/wwdos` → connect browser |
| 3 | Sprites URL → port 8080 routing | `sprite proxy 8080` then `curl localhost:8080` |
| 4 | `sprite-env services` exact syntax | Check with `sprite-env --help` on Sprite |
