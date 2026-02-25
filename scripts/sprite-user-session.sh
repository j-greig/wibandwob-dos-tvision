#!/usr/bin/env bash
# scripts/sprite-user-session.sh
# Spawned by ttyd for each browser connection.
# Each invocation gets its own wwdos process + PartyKit bridge via INST=$$.
#
# Lifecycle:
#   browser opens → ttyd forks this script
#   script starts bridge (background, waits for IPC socket)
#   script starts wwdos (foreground, attached to browser PTY)
#   browser closes → PTY closes → wwdos exits → socket removed
#   bridge detects socket gone → exits cleanly (F04 fix)

set -euo pipefail

# ── Instance ID = shell PID (guaranteed unique per connection) ────────────────
INST=$$
export WIBWOB_INSTANCE=$INST

# ── Load .env if present (secrets + config, never committed) ──────────────────
APP="${APP:-/home/sprite/app}"
if [ -f "$APP/.env" ]; then
  set -a; source "$APP/.env"; set +a
fi
export WIBWOB_PARTYKIT_URL="${WIBWOB_PARTYKIT_URL:-https://wibwob-rooms.j-greig.partykit.dev}"
export WIBWOB_PARTYKIT_ROOM="${WIBWOB_PARTYKIT_ROOM:-rchat-live}"
export WIBWOB_NO_STATE_SYNC="${WIBWOB_NO_STATE_SYNC:-1}"  # Sprites: chat+presence only
export WIBWOB_SCRAMBLE_DEFAULT="${WIBWOB_SCRAMBLE_DEFAULT:-tall}"  # Auto-open scramble chat
export WIBWOB_LAYOUT_PATH="${WIBWOB_LAYOUT_PATH:-$APP/config/sprites/default-layout.json}"

SOCK="/tmp/wibwob_${INST}.sock"
LOG="/tmp/wibwob_bridge_${INST}.log"

# ── Start bridge in background ────────────────────────────────────────────────
# Waits up to 10s for IPC socket to appear, then connects to PartyKit.
# Exits automatically when socket disappears (F04 fix in partykit_bridge.py).
(
  echo "[bridge:${INST}] waiting for IPC socket $SOCK" >> "$LOG"
  TIMEOUT=50  # 50 × 0.2s = 10s
  until [ -S "$SOCK" ] || [ $TIMEOUT -le 0 ]; do
    sleep 0.2
    TIMEOUT=$((TIMEOUT - 1))
  done
  if [ ! -S "$SOCK" ]; then
    echo "[bridge:${INST}] timeout waiting for socket — aborting" >> "$LOG"
    exit 1
  fi
  echo "[bridge:${INST}] socket ready, starting bridge" >> "$LOG"
  cd "$APP/tools/room"
  python3 partykit_bridge.py >> "$LOG" 2>&1
  echo "[bridge:${INST}] bridge exited" >> "$LOG"
) &
BRIDGE_PID=$!

# ── Start TUI in foreground (attached to browser PTY) ────────────────────────
# When browser tab closes: PTY closes → this process exits →
# socket removed → bridge detects and exits (F04 fix).
# cd to APP so relative paths (primers/, modules/, etc.) resolve correctly
cd "$APP"

WWDOS="${APP}/build-sprites/app/wwdos"
if [ ! -x "$WWDOS" ]; then
  # Fallback to dev build if sprites build not present
  WWDOS="${APP}/build/app/wwdos"
fi
exec "$WWDOS"
