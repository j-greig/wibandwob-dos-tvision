#!/bin/bash
# End-to-end IPC chain test.
# Tests every link: socket → IPC → API → commands → execution.
# Run AFTER starting TUI and API.
set -e

RED='\033[0;31m'
GRN='\033[0;32m'
YEL='\033[0;33m'
NC='\033[0m'

PASS=0
FAIL=0
pass() { echo -e "${GRN}✅ PASS${NC}: $1"; PASS=$((PASS+1)); }
fail() { echo -e "${RED}❌ FAIL${NC}: $1 — $2"; FAIL=$((FAIL+1)); }

API="http://127.0.0.1:${WIBWOB_API_PORT:-8089}"

echo "=========================================="
echo "  IPC Chain Test"
echo "=========================================="
echo ""

# ── 1. Socket exists ──
echo "── Step 1: Socket file ──"
SOCK="/tmp/wwdos.sock"
INST="${WIBWOB_INSTANCE:-}"
if [ -n "$INST" ]; then SOCK="/tmp/wibwob_${INST}.sock"; fi

if [ -S "$SOCK" ]; then
    pass "Socket exists: $SOCK"
else
    fail "Socket missing" "$SOCK not found. Is the TUI running?"
fi

# ── 2. Direct IPC works ──
echo ""
echo "── Step 2: Direct IPC (bypass API) ──"
IPC_RESP=$(echo "cmd:get_canvas_size" | nc -U "$SOCK" -w 2 2>/dev/null || echo "CONNECT_FAIL")
if echo "$IPC_RESP" | grep -q '"width"'; then
    pass "Direct IPC: get_canvas_size returns dimensions"
else
    fail "Direct IPC" "got: $IPC_RESP"
fi

# ── 3. API health ──
echo ""
echo "── Step 3: API server health ──"
HEALTH=$(curl -sf "$API/health" 2>/dev/null || echo "FAIL")
if echo "$HEALTH" | grep -q '"ok"'; then
    pass "API /health returns ok"
else
    fail "API health" "$HEALTH"
fi

# ── 4. API → IPC: get_state ──
echo ""
echo "── Step 4: API → IPC state ──"
STATE=$(curl -sf "$API/state" 2>/dev/null || echo "FAIL")
if echo "$STATE" | grep -q '"windows"'; then
    WIN_COUNT=$(echo "$STATE" | python3 -c "import json,sys; print(len(json.load(sys.stdin).get('windows',[])))" 2>/dev/null || echo "?")
    pass "API /state returns state ($WIN_COUNT windows)"
else
    fail "API /state" "$(echo "$STATE" | head -c 100)"
fi

# ── 5. API → IPC: commands registry ──
echo ""
echo "── Step 5: Command registry ──"
CMDS=$(curl -sf "$API/commands" 2>/dev/null || echo "FAIL")
CMD_COUNT=$(echo "$CMDS" | python3 -c "import json,sys; print(json.load(sys.stdin).get('count',0))" 2>/dev/null || echo "0")
if [ "$CMD_COUNT" -gt 50 ]; then
    pass "API /commands returns $CMD_COUNT commands"
else
    fail "API /commands" "only $CMD_COUNT commands (expected 70+). Full response: $(echo "$CMDS" | head -c 200)"
fi

# ── 6. Key commands present ──
echo ""
echo "── Step 6: Key commands present ──"
for CMD in open_figlet_text open_verse open_browser open_wibwob get_state cascade; do
    if echo "$CMDS" | grep -q "\"$CMD\""; then
        pass "Command '$CMD' registered"
    else
        fail "Command '$CMD'" "not found in registry"
    fi
done

# ── 7. Execute a command via API ──
echo ""
echo "── Step 7: Execute command via API ──"
EXEC_RESP=$(curl -sf -X POST "$API/menu/command" \
    -H "Content-Type: application/json" \
    -d '{"command": "get_canvas_size"}' 2>/dev/null || echo "FAIL")
if echo "$EXEC_RESP" | grep -q "width\|ok"; then
    pass "POST /menu/command get_canvas_size works"
else
    fail "POST /menu/command" "$(echo "$EXEC_RESP" | head -c 200)"
fi

# ── 8. Execute open_figlet_text ──
echo ""
echo "── Step 8: Open figlet window ──"
FIGLET_RESP=$(curl -sf -X POST "$API/menu/command" \
    -H "Content-Type: application/json" \
    -d '{"command": "open_figlet_text", "args": {"text": "TEST", "font": "standard"}}' 2>/dev/null || echo "FAIL")
if echo "$FIGLET_RESP" | grep -q '"ok"'; then
    pass "open_figlet_text created window"
    # Clean up
    sleep 0.5
    curl -sf -X POST "$API/menu/command" \
        -H "Content-Type: application/json" \
        -d '{"command": "close_all"}' >/dev/null 2>&1 || true
else
    fail "open_figlet_text" "$(echo "$FIGLET_RESP" | head -c 200)"
fi

# ── 9. Python ipc_client resolver ──
echo ""
echo "── Step 9: Python resolver ──"
PY_SOCK=$(cd /Users/james/Repos/wibandwob-dos && python3 -c "
import sys; sys.path.insert(0, 'tools/api_server')
from ipc_client import resolve_sock_path
print(resolve_sock_path())
" 2>/dev/null || echo "FAIL")
if [ "$PY_SOCK" = "$SOCK" ]; then
    pass "Python resolve_sock_path() = $PY_SOCK"
else
    fail "Python resolver" "expected $SOCK, got $PY_SOCK"
fi

# ── Summary ──
echo ""
echo "=========================================="
echo -e "  Results: ${GRN}$PASS passed${NC}, ${RED}$FAIL failed${NC}"
echo "=========================================="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
