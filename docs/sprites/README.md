# WibWob-DOS on Sprites.dev

> Host the full TUI in a browser via xterm.js — multiple users, one Sprite, zero local setup for participants.

## TL;DR

One [Sprites.dev](https://sprites.dev) microVM runs `ttyd`. Each browser
connection gets its own isolated `wwdos` process + PartyKit bridge with a
unique identity. All users land in the same chat room. Presence sidebar shows
the live count. ~35 MB RAM per user.

```
[Browser A]   [Browser B]   [Browser C]
      └──────── ttyd :8080 ────────────┘
                forks per connection
        wwdos+bridge  wwdos+bridge  wwdos+bridge
        amber-gnu     sage-jay      swift-fox
              └──────── PartyKit room ──────────┘
                         3 in room ✓
```

**Live URL**: `https://wibwob-dos-bbxc2.sprites.app`

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

# 2. Clone repo onto Sprite
sprite exec -- git clone https://<GITHUB_TOKEN>@github.com/j-greig/wibandwob-dos /home/sprite/app
sprite exec -- bash -c "cd /home/sprite/app && git checkout feat/e015-sprites-hosting"

# 3. Provision (deps, build, services)
# Copy sprite-setup.sh to Sprite and run it — or run inline:
sprite exec -- bash /home/sprite/app/scripts/sprite-setup.sh

# 4. Open to the world
sprite url update --auth public
sprite url        # → https://wibwob-dos-<id>.sprites.app

# 5. Send the URL to a friend. You both open it. Chat.
```

---

## Default layout

The Sprites layout is config-driven via `config/sprites/default-layout.json`,
loaded automatically by `sprite-user-session.sh` through the `WIBWOB_LAYOUT_PATH`
env var.

Default arrangement:
- **Left column**: 3 ASCII art primers stacked vertically (chaos-vs-order, iso-tall-cubes-emoji, symbient-protest)
- **Right column**: Room Chat window (wide, offset 2 chars from primers)

To customise: edit `config/sprites/default-layout.json` and restart the service.
No rebuild needed — the JSON is read at runtime.

See [`config/sprites/README.md`](../../config/sprites/README.md) for layout format docs.

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
- Their own identity in the presence sidebar

### Chat & presence relay

- `WIBWOB_NO_STATE_SYNC=1` (default for Sprites) — only chat messages and presence events relay between users. Window state is per-user
- `/rename <name>` changes your display name. Broadcast to all connected users via PartyKit `rename` message type
- Late joiners receive rename re-broadcasts when they connect (bridge re-sends on presence join events)

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

The `sprites` preset is Linux-only (the `-s` strip flag). On macOS, use `release`.

`sprite-user-session.sh` prefers `build-sprites/app/wwdos` and falls back to
`build/app/wwdos` if the sprites build isn't present.

---

## Maintenance

### Quick redeploy after a code push

```bash
# JSON/Python only (no rebuild needed):
sprite exec -- bash -c "cd /home/sprite/app && git pull -q"
sprite exec -- sprite-env services stop tui-host
sprite exec -- bash -c "rm -f /tmp/wibwob_*.sock /tmp/wibwob_bridge_*.log"
sprite exec -- sprite-env services start tui-host

# C++ changes (rebuild required):
sprite exec -- bash -c "cd /home/sprite/app && git pull -q && cmake --build --preset sprites --target wwdos -- -j\$(nproc)"
sprite exec -- sprite-env services stop tui-host
sprite exec -- bash -c "rm -f /tmp/wibwob_*.sock /tmp/wibwob_bridge_*.log"
sprite exec -- sprite-env services start tui-host
```

**Note**: `sprite-env services restart` returns 404 — always use stop + start.

### PartyKit server redeploy

If you change `partykit/src/server.ts` (e.g. adding new message types):
```bash
cd partykit && npx partykit deploy
```

### modules-private (primer art files)

These can't be cloned on the Sprite (nested git repo). Deploy via tar staging:
```bash
# On macOS — stage to clean dir to avoid tar + nested .git issues
rm -rf /tmp/mp-stage && mkdir -p /tmp/mp-stage/wibwob-primers/primers
cp modules-private/wibwob-primers/primers/*.txt /tmp/mp-stage/wibwob-primers/primers/
tar -C /tmp/mp-stage -cf - . | sprite exec -- bash -c "cd /home/sprite/app/modules-private && tar xf -"
```

### Check RAM headroom

```bash
sprite exec -- free -m
# Each user ≈ 35 MB (wwdos ~15 MB + bridge ~20 MB)
```

### Watch bridge logs

```bash
sprite exec -- ls /tmp/wibwob_bridge_*.log
sprite exec -- tail -f /tmp/wibwob_bridge_<PID>.log
```

### Check for zombie bridges

```bash
sprite exec -- ps aux | grep partykit_bridge
```

---

## Gotchas learned from live deploys

| Gotcha | Fix |
|--------|-----|
| tvision `command==0` = "has submenu" → null deref crash on ANY keypress | Use `cmNoOp` (999) for placeholder menu items, never 0 |
| `workspaces/` is gitignored | Layout JSON lives in `config/sprites/` instead |
| macOS `tar` + nested `.git` dirs → empty directories on extract | Stage files to clean `/tmp/` dir first, then tar |
| `sprite-env services restart` → 404 | Use `stop` then `start` |
| FIGlet font change freezes TUI | `popen()` blocks main thread. Known issue, needs async fix |
| Rename messages dropped by PartyKit | Must add each message type to `onMessage` switch in `server.ts` and redeploy |
| Late joiners don't see existing custom names | Bridge re-broadcasts rename on join events |

---

## Key files

| File | Purpose |
|------|---------|
| `scripts/sprite-setup.sh` | One-time Sprite provisioning (deps → build → services) |
| `scripts/sprite-user-session.sh` | Per-connection wrapper (INST=$$, bridge wait, TUI exec) |
| `config/sprites/default-layout.json` | Default window layout for Sprites |
| `tools/room/partykit_bridge.py` | Bridge — chat relay, presence, rename broadcast |
| `partykit/src/server.ts` | PartyKit server — relays chat, presence, rename |
| `CMakePresets.json` | dev / sprites / release build presets |
| `.planning/epics/e015-sprites-hosting/` | Epic brief + architecture debate |
| `.planning/epics/e016-shared-experience/` | Multi-user experience epic (F01–F03) |

---

## Architecture decisions

See the full debate record:
[`.planning/epics/e015-sprites-hosting/debate/`](../../.planning/epics/e015-sprites-hosting/debate/)

| Decision | Choice | Why |
|----------|--------|-----|
| Process model | Per-connection fork (ttyd default) | Each user = own PartyKit identity |
| Instance ID | `WIBWOB_INSTANCE=$$` | Shell PID, unique per session |
| API server | Not in per-user path | Humans use PTY, not REST API |
| State sync | Disabled (`WIBWOB_NO_STATE_SYNC=1`) | Chat + presence only; window state per-user |
| Room name | Fixed `rchat-live` env var | Zero coordination; lobby is V2 |
| Sprite count | 1 | ttyd + N forked children, ~35 MB/user |
| Layout | Config-driven JSON workspace | No C++ rebuilds for layout tweaks |
