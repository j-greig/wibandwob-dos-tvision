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
sprite exec -- bash -c "cd /home/sprite/app && git pull && cmake --build build-sprites --target wwdos -j$(nproc)"
```

## Full provision (fresh Sprite)

```bash
.claude/skills/sprites-deploy/scripts/provision.sh
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
git submodule update --init vendor/tvterm
git -C vendor/tvterm submodule update --init --recursive   # gets vterm
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

`git clone SimHacker/MicropolisCore` at HEAD has a different dir structure
than the pinned submodule commit. Always checkout the exact hash:

```bash
cd vendor/MicropolisCore
git checkout e32fb8689b4eb6c9c2dd890df57ef3fd5dc1090d
```

CMakeLists.txt expects `vendor/MicropolisCore/MicropolisEngine/src/*.cpp`.
Verify with: `ls vendor/MicropolisCore/MicropolisEngine/src/*.cpp | wc -l` → 27

### ⚠️ CMakePresets.json must be on the branch being built

`CMakePresets.json` lives on feature branches until merged to main.
Always checkout the right branch on the Sprite before building:

```bash
git fetch origin feat/e015-sprites-hosting
git checkout feat/e015-sprites-hosting
git pull
```

### ⚠️ `ls | head -N` hides files — MicropolisEngine appeared missing

`ls /dir | head -5` truncated the listing. `MicropolisEngine/` was present
but cut off. Always verify files exist explicitly:

```bash
ls vendor/MicropolisCore/MicropolisEngine/src/*.cpp | wc -l   # expect 27
```

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
Just retry — it resolves on second attempt. Not a real error.

### ⚠️ `apt-get install` needs `sudo`

Despite running as the `sprite` user, apt needs sudo:

```bash
sudo apt-get install -y cmake ttyd build-essential libncurses-dev
```

### ⚠️ cmake and ttyd not preinstalled

Sprites Ubuntu 24.04 ships with: gcc, Python, Node, Go, git, curl.
**Not preinstalled**: cmake, ttyd. Always install both at provision time.

### ⚠️ Use `cmake --preset sprites` on Linux for stripped release binary

`CMakePresets.json` has a `sprites` preset (Release + stripped, Linux only).
Build dir is `build-sprites/` — kept separate from dev `build/`.

```bash
cmake --preset sprites      # configure (Linux only — has -s strip flag)
cmake --build --preset sprites -- -j$(nproc)
# binary → build-sprites/app/wwdos
```

### ✅ `sprite-env services` confirmed syntax

```bash
sprite exec -- sprite-env services create tui-host \
  --cmd ttyd \
  --args '--port,8080,--writable,/home/sprite/app/scripts/sprite-user-session.sh' \
  --env 'WIBWOB_PARTYKIT_ROOM=rchat-live,WIBWOB_PARTYKIT_URL=https://wibwob-rooms.j-greig.partykit.dev,APP=/home/sprite/app' \
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

### ⚠️ RAM is generous — 16GB on tested Sprite

Each user costs ~35MB (wwdos ~15MB + bridge ~20MB).
16GB = ~450 concurrent users. Not a constraint.

---

## Sprite specs (observed)

| Thing | Value |
|-------|-------|
| OS | Ubuntu 24.04 LTS |
| RAM | 16 GB |
| Disk | 99 GB |
| Preinstalled | gcc 14, Python 3, Node, Go, git, curl |
| NOT preinstalled | cmake, ttyd, uv |
| URL format | `https://<name>-<org>.sprites.app` |
| HTTP default port | 8080 |
| Auth modes | `sprite` (org only) / `public` (anyone) |

---

## Key commands

```bash
sprite create <name>                    # create new Sprite
sprite use <name>                       # set active
sprite list                             # list all Sprites
sprite exec -- <cmd>                    # run command
sprite exec -tty -- <cmd>             # interactive TTY
sprite console                          # interactive shell
sprite url                              # show URL
sprite url update --auth public        # make public
sprite proxy 8080                       # forward port locally for testing
sprite checkpoint create               # snapshot filesystem
sprite restore                          # roll back to checkpoint
sprite destroy                          # ⚠️ IRREVERSIBLE — deletes everything
```

---

## Architecture reminder

```
Browser → ttyd :8080 → forks sprite-user-session.sh per connection
                          INST=$$ → unique socket /tmp/wibwob_$$.sock
                          wwdos (foreground, PTY)
                          partykit_bridge.py (background, waits for socket)
                          All bridges → same PartyKit room
```

See full debate: `.planning/epics/e015-sprites-hosting/debate/round-4-agreed.md`

---

## Files

| File | Role |
|------|------|
| `scripts/sprite-setup.sh` | Full provision script |
| `scripts/sprite-user-session.sh` | Per-connection ttyd wrapper |
| `tools/room/partykit_bridge.py` | Bridge — exits cleanly when TUI socket gone |
| `CMakePresets.json` | `sprites` / `dev` / `release` build presets |
| `docs/sprites/README.md` | Full architecture + maintenance docs |

---

### ⚠️ figlet not preinstalled

```bash
sudo apt-get install -y figlet
```

### ⚠️ `TMenuItem("label", 0, kbNoKey)` crashes tvision on ANY keypress

tvision treats `command == 0` as "this item has a submenu" and dereferences
`subMenu->items` without null check. The menu bar scans ALL keypresses for
hotkeys, so this crashes on every single keypress in every view.
Fix: use `cmOK` (10) or any non-zero command instead.

### ⚠️ GDB debug wrapper — use per-PID log files

When using ttyd + gdb wrapper, multiple sessions overwrite the same log.
Always use `$$.log`:

```bash
LOG=/tmp/gdb-crash-$$.log
gdb -batch -ex 'run' -ex 'bt full' --args ./build-debug/app/wwdos > $LOG 2>&1
```

Remember to switch back to the release binary + normal session script after
debugging (the service remembers the gdb wrapper command).

## TODO / still to verify on live Sprite

- [x] Confirm `sprite-env services` exact syntax — `--args` comma-separated
- [x] Confirm Sprite URL routes to port 8080 — `--http-port 8080` on service
- [x] Confirm tvision renders correctly inside ttyd PTY — works
- [x] Confirm 2 browser windows → 2 different adjective-animal names — works
- [x] Typing in any view works (was crashing due to null submenu bug)
- [ ] Confirm chat relays between the 2 windows
- [ ] Confirm bridge exits cleanly when browser tab closed (no zombie)
