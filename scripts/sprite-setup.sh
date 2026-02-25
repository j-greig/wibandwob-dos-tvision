#!/usr/bin/env bash
# scripts/sprite-setup.sh
# One-time provisioning of a Sprites.dev microVM for WibWob-DOS.
# Run once after `sprite create wibwob-dos && sprite use wibwob-dos`.
#
# Usage (from local machine):
#   sprite exec -- bash /tmp/sprite-setup.sh
# Or upload and run:
#   cat scripts/sprite-setup.sh | sprite exec -- bash -s

set -euo pipefail
APP=/home/sprite/app
REPO="${WIBWOB_REPO:-https://github.com/j-greig/wibandwob-dos}"
ROOM="${WIBWOB_PARTYKIT_ROOM:-rchat-live}"
PK_URL="${WIBWOB_PARTYKIT_URL:-https://wibwob-rooms.j-greig.partykit.dev}"

echo "━━━ WibWob-DOS Sprite Setup ━━━"
echo "Repo:  $REPO"
echo "Room:  $ROOM"
echo "PK:    $PK_URL"
echo ""

# ── 1. System deps ─────────────────────────────────────────────────────────
echo "▸ system deps..."
apt-get update -qq
apt-get install -y --no-install-recommends \
  ttyd cmake build-essential git libncurses-dev 2>&1 | tail -3

# ── 2. Clone repo ──────────────────────────────────────────────────────────
echo "▸ cloning repo → $APP"
if [[ -d "$APP/.git" ]]; then
  git -C "$APP" pull --ff-only
else
  git clone "$REPO" "$APP"
fi

# ── 3. Build TUI binary (sprites preset = Release + stripped, Linux only) ──
echo "▸ building wwdos..."
cd "$APP"
if cmake --preset sprites 2>/dev/null; then
  cmake --build --preset sprites -- -j$(nproc) 2>&1 | grep -E "error:|Built target wwdos|Linking CXX exec"
  WWDOS_BIN="$APP/build-sprites/app/wwdos"
else
  # Fallback: plain Release (macOS CI, older cmake)
  cmake -S "$APP" -B "$APP/build-sprites" -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -2
  cmake --build "$APP/build-sprites" --target wwdos -j$(nproc) 2>&1 | grep -E "error:|Built target wwdos"
  WWDOS_BIN="$APP/build-sprites/app/wwdos"
fi
echo "  binary: $WWDOS_BIN"

# ── 4. Python deps ─────────────────────────────────────────────────────────
echo "▸ pip install websockets..."
pip install websockets --quiet

# ── 5. Make session script executable ─────────────────────────────────────
chmod +x "$APP/scripts/sprite-user-session.sh"

# ── 6. Register ttyd as a Service (auto-restarts on Sprite wake) ───────────
echo "▸ registering ttyd service..."
sprite-env services create tui-host \
  --cmd ttyd \
  --args "--port 8080 --writable $APP/scripts/sprite-user-session.sh" \
  --env "WIBWOB_PARTYKIT_ROOM=$ROOM,WIBWOB_PARTYKIT_URL=$PK_URL,APP=$APP" \
  2>/dev/null && echo "  service registered" || echo "  (service may already exist)"

# ── 7. RAM check ───────────────────────────────────────────────────────────
echo ""
echo "━━━ Sprite stats ━━━"
free -m | grep Mem
df -h / | tail -1
echo ""
echo "━━━ Done ━━━"
echo "Next:"
echo "  sprite url update --auth public"
echo "  sprite url   # → your public HTTPS URL"
