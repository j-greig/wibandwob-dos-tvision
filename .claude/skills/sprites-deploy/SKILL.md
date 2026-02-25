---
name: sprites-deploy
description: Deploy and manage WibWob-DOS on Sprites.dev. Covers provisioning, build, services, URL, and gotchas learned from live deploys. Use when deploying to Sprites, debugging a Sprite, or updating the hosted instance. Triggers on "deploy to sprites", "sprites deploy", "update sprite", "sprite hosting", "provision sprite".
---

# sprites-deploy

Live deployment runbook for WibWob-DOS on Sprites.dev.
Gotchas marked ⚠️ — learned from actual deploys, not theory.

## Quick re-deploy (Sprite already provisioned)

```bash
sprite use wibwob-dos
sprite exec -- bash -c "cd /home/sprite/app && git pull && cmake --build build-sprites --target wwdos -j\$(nproc)"
# Then restart service:
sprite exec -- sprite-env services stop tui-host
sprite exec -- rm -f /tmp/wibwob_*.sock /tmp/wibwob_bridge_*.log
sprite exec -- sprite-env services start tui-host
```

## Full provision checklist (fresh Sprite)

```bash
sprite create wibwob-dos && sprite use wibwob-dos
sprite exec -- git clone https://github.com/j-greig/wibandwob-dos /home/sprite/app
sprite exec -- bash -c "cd /home/sprite/app && git checkout feat/e015-sprites-hosting"
# System deps
sprite exec -- sudo apt-get install -y cmake ttyd figlet
# Python deps
sprite exec -- pip install websockets
# Submodules (skip modules-private — use tar pipe below)
sprite exec -- bash -c "cd /home/sprite/app && git submodule deinit modules-private 2>/dev/null; true"
sprite exec -- bash -c "cd /home/sprite/app && git submodule update --init vendor/tvision"
sprite exec -- bash -c "cd /home/sprite/app && rm -rf vendor/tvterm && git clone https://github.com/j-greig/tvterm.git vendor/tvterm && cd vendor/tvterm && git submodule update --init --recursive"
sprite exec -- bash -c "cd /home/sprite/app && git submodule update --init vendor/MicropolisCore"
# Build
sprite exec -- bash -c "cd /home/sprite/app && cmake --preset sprites && cmake --build --preset sprites --target wwdos -- -j\$(nproc)"
# Pipe modules-private (see gotcha below)
# Register ttyd service (see service creation below)
sprite url update --auth public
```

---

## Gotchas — read before doing anything

### ⚠️ No uv on Sprites — use pip + python3 directly

Sprites has Python preinstalled. The bridge (`partykit_bridge.py`) uses
a `# /// script` inline dep header for local `uv run` convenience.
On the Sprite just use:

```bash
pip install websockets      # once at provision time
python3 partykit_bridge.py  # in session script
```

Don't install uv — extra complexity for no gain, filesystem persists so
pip install is permanent anyway.

### ⚠️ `git submodule update --init --recursive` chokes on private submodules

The repo has a private submodule (`modules-private`) that blocks recursive
init. Init vendor submodules explicitly by path, skipping private ones:

```bash
cd /home/sprite/app
git submodule deinit modules-private 2>/dev/null || true
git submodule update --init vendor/tvision
# tvterm: see next gotcha
git submodule update --init vendor/MicropolisCore
```

### ⚠️ `git submodule update --init` silently no-ops on a non-empty dir

If `git submodule update --init vendor/tvterm` exits 0 but the dir is empty
(or partially cloned), remove and clone directly:

```bash
rm -rf vendor/tvterm
git clone https://github.com/j-greig/tvterm.git vendor/tvterm
cd vendor/tvterm && git submodule update --init --recursive
```

Upstream fallback if j-greig/tvterm is unavailable:
`https://github.com/magiblot/tvterm.git`

### ⚠️ MicropolisCore needs specific commit checkout

If the directory structure looks wrong after clone, checkout the pinned hash:

```bash
cd vendor/MicropolisCore
git checkout e32fb8689b4eb6c9c2dd890df57ef3fd5dc1090d
```

CMakeLists.txt expects `vendor/MicropolisCore/MicropolisEngine/src/*.cpp`.
Verify: `ls vendor/MicropolisCore/MicropolisEngine/src/*.cpp | wc -l` → 27

### ⚠️ CMakePresets.json must be on the branch being built

`CMakePresets.json` lives on feature branches until merged to main.
Always checkout the right branch on the Sprite before building.

### ⚠️ `ls | head -N` hides files

`ls /dir | head -5` truncated the listing and made MicropolisEngine
appear missing. Always verify files exist explicitly with `wc -l`.

### ⚠️ `modules-private` — tar + pipe, don't clone

Private submodule can't clone on Sprite (no GitHub auth for submodule
fetches). And it has a nested git repo (`primers/`) that macOS tar
won't descend into. Stage to a clean dir first, then tar:

```bash
# Local (macOS)
rm -rf /tmp/mp-stage
mkdir -p /tmp/mp-stage/modules-private/wibwob-primers/primers
mkdir -p /tmp/mp-stage/modules-private/wibwob-prompts
cp modules-private/README.md /tmp/mp-stage/modules-private/
cp modules-private/wibwob-primers/module.json /tmp/mp-stage/modules-private/wibwob-primers/
cp modules-private/wibwob-prompts/*.md /tmp/mp-stage/modules-private/wibwob-prompts/
cp modules-private/wibwob-primers/primers/*.txt /tmp/mp-stage/modules-private/wibwob-primers/primers/
cd /tmp/mp-stage && COPYFILE_DISABLE=1 tar czf /tmp/modules-private.tar.gz modules-private/

# Pipe to Sprite
cat /tmp/modules-private.tar.gz | sprite exec -- bash -c "cd /home/sprite/app && rm -rf modules-private && tar xzf -"
```

### ⚠️ git DNS works but `git clone` fails — retry

Sprites can intermittently fail DNS inside `git` even when `curl` works.
Just retry — it resolves on second attempt.

### ⚠️ `apt-get install` needs `sudo`

```bash
sudo apt-get install -y cmake ttyd figlet build-essential libncurses-dev
```

### ⚠️ cmake, ttyd, figlet not preinstalled

Sprites Ubuntu 24.04 ships with: gcc, Python, Node, Go, git, curl.
**Not preinstalled**: cmake, ttyd, figlet. Install all three at provision time.

### ⚠️ Use `cmake --preset sprites` on Linux for stripped release binary

Build dir is `build-sprites/` — kept separate from dev `build/`.

```bash
cmake --preset sprites
cmake --build --preset sprites -- -j$(nproc)
# binary → build-sprites/app/wwdos
```

### ✅ `sprite-env services` confirmed syntax

```bash
sprite exec -- sprite-env services create tui-host \
  --cmd ttyd \
  --args '--port,8080,--writable,/home/sprite/app/scripts/sprite-user-session.sh' \
  --env 'WIBWOB_PARTYKIT_ROOM=rchat-live,WIBWOB_PARTYKIT_URL=https://wibwob-rooms.j-greig.partykit.dev,APP=/home/sprite/app,WIBWOB_NO_STATE_SYNC=1' \
  --dir /home/sprite/app \
  --http-port 8080
```

Key: `--args` is **comma-separated** (not space-separated).
Key: `--http-port` routes the Sprite URL to that port AND auto-starts the
service when an HTTP request arrives. Only one service can own `--http-port`.

### ⚠️ `sprite-env services restart` returns 404

Use stop + start instead:

```bash
sprite exec -- sprite-env services stop tui-host
sprite exec -- sprite-env services start tui-host
```

### ⚠️ Always clean stale sockets before restart

ttyd kills processes with SIGHUP but doesn't clean up Unix sockets:

```bash
sprite exec -- rm -f /tmp/wibwob_*.sock /tmp/wibwob_bridge_*.log
```

---

## wwdos-specific Sprites gotchas

### ⚠️ `TMenuItem("label", 0, kbNoKey)` crashes tvision on ANY keypress

tvision treats `command == 0` as "this item has a submenu" and dereferences
`subMenu->items` without null check in `findHotKey()`. The menu bar scans
ALL keypresses for hotkeys, so this crashes on every keypress in every view.

Fix: use `cmNoOp` (999, defined in `room_chat_view.h`) instead of 0.
Regression test: `tests/test_menu_null_submenu.sh` (static source scan).

### ⚠️ `WIBWOB_NO_STATE_SYNC=1` required for multi-user Sprites

Without this, both bridges push window deltas to the same PartyKit room.
Results in: resize ping-pong, foreign windows appearing, crashes.
Set in `sprite-user-session.sh` as default. Chat + presence still relay.

### ⚠️ Session script must `cd "$APP"` before exec wwdos

wwdos uses relative paths for `primers/`, `modules/`, `modules-private/`.
Without `cd`, these resolve to `/home/sprite/primers` → file not found.

### ⚠️ WibWobEngine doesn't pick up runtime API key

Scramble sidebar uses `scrambleEngine.setApiKey()` directly, but WibWobEngine
uses `AuthConfig::instance()` which is set at startup and never refreshed.
Fix: broadcast `cmApiKeyChanged` (186) from the API key dialog; WibWobView
handles it and calls `engine->setApiKey()`. Constant defined in `room_chat_view.h`.

### ⚠️ FIGlet font change locks the window

Known bug on Sprites. Default font works. Font switching hangs. Not yet
diagnosed — may be a PATH issue or subprocess timeout. Track separately.

### ⚠️ GDB debug wrapper — use per-PID log files

When using ttyd + gdb wrapper, multiple sessions overwrite the same log.
Always use `$$.log`:

```bash
LOG=/tmp/gdb-crash-$$.log
gdb -batch -ex 'run' -ex 'bt full' --args ./build-debug/app/wwdos > $LOG 2>&1
```

Also add `handle SIGWINCH nostop noprint pass` to avoid gdb catching
terminal resize signals.

Remember to switch back to the release binary + normal session script after
debugging (the service remembers the gdb wrapper command):

```bash
sprite exec -- sprite-env services delete tui-host
# Then re-create with the normal session script (see service creation above)
```

---

## Sprite specs (observed)

| Thing | Value |
|-------|-------|
| OS | Ubuntu 24.04 LTS |
| RAM | 16 GB |
| Disk | 99 GB |
| Preinstalled | gcc 14, Python 3, Node, Go, git, curl |
| NOT preinstalled | cmake, ttyd, figlet |
| URL format | `https://<name>-<id>.sprites.app` |
| HTTP default port | 8080 |
| Auth modes | `sprite` (org only) / `public` (anyone) |
| Live URL | `https://wibwob-dos-bbxc2.sprites.app` |

---

## Key commands

```bash
sprite create <name>                    # create new Sprite
sprite use <name>                       # set active
sprite list                             # list all Sprites
sprite exec -- <cmd>                    # run command
sprite exec -tty -- <cmd>              # interactive TTY
sprite console                          # interactive shell
sprite url                              # show URL
sprite url update --auth public         # make public
sprite proxy 8080                       # forward port locally for testing
sprite checkpoint create                # snapshot filesystem
sprite restore                          # roll back to checkpoint
sprite destroy                          # ⚠️ IRREVERSIBLE — deletes everything
```

---

## Architecture

```
Browser → ttyd :8080 → forks sprite-user-session.sh per connection
                          INST=$$ → unique socket /tmp/wibwob_$$.sock
                          cd $APP (fixes relative paths)
                          wwdos (foreground, PTY)
                          partykit_bridge.py (background, waits for socket)
                          WIBWOB_NO_STATE_SYNC=1 → chat + presence only
                          All bridges → same PartyKit room (rchat-live)
```

E016 plans two modes:
- **F01 (current):** Independent TUIs + shared chat room
- **F02 (future):** Single shared wwdos via tmux shared session

See: `.planning/epics/e016-shared-experience/e016-epic-brief.md`

---

## Files

| File | Role |
|------|------|
| `scripts/sprite-setup.sh` | Full provision script |
| `scripts/sprite-user-session.sh` | Per-connection ttyd wrapper |
| `tools/room/partykit_bridge.py` | Bridge — chat + presence relay, exits on socket gone |
| `CMakePresets.json` | `sprites` / `dev` / `release` build presets |
| `docs/sprites/README.md` | Full architecture + maintenance docs |
| `tests/test_menu_null_submenu.sh` | Regression: no command=0 in menu items |

---

## Verified ✅ / still to verify

- [x] `sprite-env services` exact syntax — `--args` comma-separated
- [x] Sprite URL routes to port 8080 — `--http-port 8080` on service
- [x] tvision renders correctly inside ttyd PTY
- [x] 2 browser windows → 2 different adjective-animal names
- [x] Typing in any view works (null submenu crash fixed)
- [x] Primers load from `modules-private/wibwob-primers/primers/`
- [x] FIGlet renders with default font
- [x] State sync disabled — no foreign windows, no resize ping-pong
- [ ] Chat messages relay between 2 windows
- [ ] `/rename` custom name appears in other user's view
- [ ] Bridge exits cleanly when browser tab closed (no zombie)
- [ ] FIGlet font change (known broken, separate bug)
