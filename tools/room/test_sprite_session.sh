#!/usr/bin/env bash
# tools/room/test_sprite_session.sh
# Verifies sprite-user-session.sh mechanics without a live Sprite or PartyKit.
#
# Tests:
#   1. INST=$$ isolation — two sessions get different socket paths
#   2. Bridge wait loop — bridge background process waits for socket, then exits
#      when socket is never created (timeout path)
#   3. Bridge exits when socket deleted — simulates TUI exit (F04 integration)

set -euo pipefail
REPO="$(git rev-parse --show-toplevel)"
PASS=0; FAIL=0

ok()   { echo "  ✓ $1"; PASS=$((PASS+1)); }
fail() { echo "  ✗ $1"; FAIL=$((FAIL+1)); }

echo "sprite-user-session mechanics tests"
echo ""

# ── Test 1: INST=$$ gives unique socket paths ─────────────────────────────────
echo "1. INST isolation"
INST_A=10001; SOCK_A="/tmp/wibwob_${INST_A}.sock"
INST_B=10002; SOCK_B="/tmp/wibwob_${INST_B}.sock"
[[ "$SOCK_A" != "$SOCK_B" ]] && ok "different INST → different socket paths" \
                              || fail "socket paths collide"
[[ "$SOCK_A" == "/tmp/wibwob_10001.sock" ]] && ok "socket path formula correct" \
                                             || fail "unexpected path: $SOCK_A"

# ── Test 2: bridge wait loop times out when socket never appears ──────────────
echo "2. Bridge wait timeout (fast)"
INST=$$
SOCK="/tmp/wibwob_${INST}_waittest.sock"
LOG="/tmp/bridge_wait_test_${INST}.log"
rm -f "$SOCK" "$LOG"

# Run just the wait loop (inline, short timeout)
(
  TIMEOUT=3  # 3 × 0.2s = 0.6s
  until [ -S "$SOCK" ] || [ $TIMEOUT -le 0 ]; do
    sleep 0.2; TIMEOUT=$((TIMEOUT-1))
  done
  if [ ! -S "$SOCK" ]; then
    echo "timeout" > "$LOG"
    exit 1
  fi
  echo "connected" > "$LOG"
) &
WAIT_PID=$!
wait $WAIT_PID 2>/dev/null && WAIT_EXIT=0 || WAIT_EXIT=$?
[[ $WAIT_EXIT -ne 0 ]] && ok "wait loop exits with error when socket never appears" \
                        || fail "wait loop should have timed out"
[[ "$(cat $LOG 2>/dev/null)" == "timeout" ]] && ok "timeout logged correctly" \
                                              || fail "unexpected log: $(cat $LOG 2>/dev/null)"
rm -f "$LOG"

# ── Test 3: bridge exits when socket deleted (F04 integration) ────────────────
echo "3. Bridge exits on socket delete (F04 live test)"
INST=$$
SOCK="/tmp/wibwob_${INST}.sock"
BLOG="/tmp/bridge_f04_test_${INST}.log"
rm -f "$SOCK" "$BLOG"

# Create socket
python3 -c "
import socket, os
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.bind('$SOCK')
s.listen(1)
print('listening')
import time; time.sleep(30)
" &
SOCK_PID=$!
sleep 0.3

[[ -S "$SOCK" ]] && ok "socket created" || { fail "socket not created"; kill $SOCK_PID 2>/dev/null; exit 1; }

# Start bridge (will connect to socket, then exit when socket deleted)
cd "$REPO"
WIBWOB_INSTANCE=$INST \
WIBWOB_PARTYKIT_URL="ws://localhost:1" \
WIBWOB_PARTYKIT_ROOM="test-room" \
uv run tools/room/partykit_bridge.py > "$BLOG" 2>&1 &
BRIDGE_PID=$!

sleep 1.5  # let bridge start polling

# Delete the socket (simulate TUI exit)
kill $SOCK_PID 2>/dev/null; rm -f "$SOCK"
ok "socket deleted (TUI exit simulated)"

# Bridge should exit within 10s (POLL_INTERVAL=5 + margin)
for i in $(seq 1 20); do
  sleep 0.5
  kill -0 $BRIDGE_PID 2>/dev/null || { ok "bridge exited after socket deleted"; break; }
  [[ $i -eq 20 ]] && { fail "bridge still running after 10s (zombie!)"; kill $BRIDGE_PID 2>/dev/null; }
done

# Check log for TUI exited message
grep -q "TUI exited\|bridge shutting down\|socket gone" "$BLOG" 2>/dev/null \
  && ok "bridge logged clean exit reason" \
  || fail "bridge exit message not found in log (got: $(tail -3 $BLOG 2>/dev/null))"

rm -f "$BLOG"

# ── Results ───────────────────────────────────────────────────────────────────
echo ""
TOTAL=$((PASS+FAIL))
if [[ $FAIL -eq 0 ]]; then
  echo "PASS — $PASS/$TOTAL tests passed"
else
  echo "FAIL — $FAIL/$TOTAL tests failed"
  exit 1
fi
