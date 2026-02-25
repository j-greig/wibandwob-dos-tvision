# Round 4 — AGREED architecture

3 rounds, 1 confirmed bug. Architecture settled.

---

## Agreed: single Sprite, ttyd per-connection fork, `WIBWOB_INSTANCE=$$`

```
                    sprites.app HTTPS URL
                           │
                    ttyd :8080 (default, no --once)
                    forks sprite-user-session.sh per browser connection
                           │
          ┌────────────────┼────────────────┐
     connection A     connection B     connection C
     INST=101         INST=247         INST=389
     wwdos (15MB)     wwdos (15MB)     wwdos (15MB)
     bridge (20MB)    bridge (20MB)    bridge (20MB)
     /tmp/wibwob_     /tmp/wibwob_     /tmp/wibwob_
       101.sock         247.sock         389.sock
          │                  │                  │
          └──────────────────┴──────────────────┘
                       PartyKit room
                      "rchat-live" (fixed)
                        3 in room ✓
                    amber-gnu / sage-jay / swift-fox
```

---

## The wrapper script (the key artefact)

`scripts/sprite-user-session.sh` — spawned by ttyd per browser connection:

```bash
#!/usr/bin/env bash
set -euo pipefail
INST=$$
export WIBWOB_INSTANCE=$INST
export WIBWOB_PARTYKIT_URL=https://wibwob-rooms.j-greig.partykit.dev
export WIBWOB_PARTYKIT_ROOM=${WIBWOB_PARTYKIT_ROOM:-rchat-live}

# Start bridge in background — waits for socket, then exits if socket gone
(
  TIMEOUT=30
  until [ -S /tmp/wibwob_${INST}.sock ] || [ $TIMEOUT -le 0 ]; do
    sleep 0.2; TIMEOUT=$((TIMEOUT-1))
  done
  [ -S /tmp/wibwob_${INST}.sock ] || exit 1
  uv run /home/sprite/app/tools/room/partykit_bridge.py
) &
BRIDGE_PID=$!

# TUI runs in foreground — PTY attached to browser xterm.js via ttyd
exec /home/sprite/app/build/app/wwdos

# When browser closes: PTY closes → wwdos exits → socket removed
# Bridge exits on next poll cycle (see bug fix below)
```

---

## One required code fix: bridge must exit when socket file disappears

**File**: `tools/room/partykit_bridge.py` line ~251

Currently:
```python
except (OSError, ConnectionRefusedError, EOFError) as e:
    self.log(f"event subscribe dropped ({e}), retrying in {EVENT_RETRY_DELAY}s")
    await asyncio.sleep(EVENT_RETRY_DELAY)
```

Fix — distinguish transient (retry) from terminal (socket file gone → exit):
```python
except (OSError, ConnectionRefusedError, EOFError) as e:
    if not os.path.exists(self.sock_path):
        self.log("IPC socket gone — TUI exited, bridge shutting down")
        return   # exits poll_loop → run() completes → process exits
    self.log(f"event subscribe dropped ({e}), retrying in {EVENT_RETRY_DELAY}s")
    await asyncio.sleep(EVENT_RETRY_DELAY)
```

Same fix needed in `poll_loop` wherever `OSError` is caught on the socket.
Without this: zombie bridge processes accumulate on the Sprite after users disconnect.

---

## Sprites setup (single Sprite, one-time)

```bash
sprite create wibwob-dos
sprite use wibwob-dos

# Provision
sprite exec -- apt-get install -y ttyd cmake build-essential git
sprite exec -- git clone https://github.com/j-greig/wibandwob-dos /home/sprite/app
sprite exec -- bash -c "cd /home/sprite/app && cmake -B build && cmake --build build --target wwdos"
sprite exec -- bash -c "cd /home/sprite/app/tools/api_server && uv sync"
sprite exec -- bash -c "cd /home/sprite/app && uv sync"

# Check RAM ceiling
sprite exec -- free -m

# Register ttyd as a Service (auto-restarts on Sprite wake)
sprite-env services create tui-host \
  --cmd ttyd \
  --args "--port 8080 --writable /home/sprite/app/scripts/sprite-user-session.sh" \
  --env "WIBWOB_PARTYKIT_ROOM=rchat-live"

# Make URL public
sprite url update --auth public
sprite url   # → https://wibwob-dos-<org>.sprites.app
```

---

## What each feature maps to

| Feature | What it is |
|---------|-----------|
| F01 Sprite setup | `scripts/sprite-setup.sh` — provision + build |
| F02 Services wiring | ttyd as Service, no API server per user |
| F03 ttyd integration | `scripts/sprite-user-session.sh` wrapper |
| F04 Room coordination | `WIBWOB_PARTYKIT_ROOM=rchat-live` fixed env var |
| F05 GitHub redeploy | GH Action: `sprite exec -- git pull && cmake --build` |
| F06 Bridge exit fix | `partykit_bridge.py` socket-gone = terminal, not retry |

---

## Unresolved (implementation, not architecture)

| # | Question | How to answer |
|---|----------|--------------|
| 1 | Sprite RAM ceiling | `sprite exec -- free -m` on real Sprite |
| 2 | tvision ncurses in ttyd PTY | `sprite exec -tty -- ttyd tmux` smoke test |
| 3 | Sprites HTTP port | Does `--port 8080` route via sprites.app URL? Confirm with live test |
| 4 | `sprite-env services` exact syntax | Verify against installed CLI |

---

## Not in scope (park for V2)

- Per-session room names (V2: lobby API)
- API server per user (only needed for agents, not humans)  
- Web chat fallback for non-TUI users (PartyKit WS direct — separate feature)
- Window mirroring across users (E008 follow-on)
