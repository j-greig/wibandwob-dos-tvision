#!/bin/bash
# smoke_all.sh — exercise every open_* command + core surfaces against the
# live app; verify each actually registers a window in /state. Run after
# any refactor touching spawn paths. Exit non-zero on any failure.
set -u
API=http://localhost:8089
PASS=0; FAIL=0; FAILED=""
cmd() { curl -s -m 10 -X POST $API/menu/command -H 'Content-Type: application/json' -d "$1"; }
wincount() { curl -s -m 5 $API/state | python3 -c "import json,sys; print(len(json.load(sys.stdin)['windows']))" 2>/dev/null || echo -1; }

check_open() {  # name payload
  local before after newest
  before=$(wincount)
  local out; out=$(cmd "$2")
  sleep 0.7
  after=$(wincount)
  if [ "$after" -gt "$before" ] 2>/dev/null; then
    PASS=$((PASS+1)); echo "PASS  $1"
    newest=$(curl -s $API/state | python3 -c "import json,sys; ws=json.load(sys.stdin)['windows']; print(max(ws,key=lambda w:int(w['id'][1:]))['id'])")
    cmd "{\"command\":\"close_window\",\"args\":{\"id\":\"$newest\"}}" >/dev/null
  else
    FAIL=$((FAIL+1)); FAILED="$FAILED $1"; echo "FAIL  $1  ($out)"
  fi
  sleep 0.3
}

check_ok() {  # name payload — expects ok:true, no window
  local out; out=$(cmd "$2")
  if echo "$out" | grep -q '"ok":true'; then PASS=$((PASS+1)); echo "PASS  $1"
  else FAIL=$((FAIL+1)); FAILED="$FAILED $1"; echo "FAIL  $1  ($out)"; fi
}

echo "── window spawns ──"
for c in open_verse open_orbit open_mycelium open_torus open_cube open_life \
         open_blocks open_score open_ascii open_animated_gradient open_gradient \
         open_monster_cam open_monster_portal open_monster_verse open_backrooms_tv \
         open_apps open_gallery open_disks open_shader open_browser open_terminal \
         open_text_editor open_quadra open_snake open_rogue open_deep_signal \
         open_micropolis_ascii new_paint_canvas open_room_chat open_wibwob; do
  check_open "$c" "{\"command\":\"$c\"}"
done
check_open open_primer '{"command":"open_primer","args":{"path":"modules/wibwob-primers/primers/standort-card.txt"}}'
check_open open_figlet_text '{"command":"open_figlet_text","args":{"text":"smoke"}}'

echo "── stateless surfaces ──"
check_ok list_skins '{"command":"list_skins"}'
check_ok reload_skins '{"command":"reload_skins"}'
for s in dflat turbo terra pipeline phosphor hercules paper midnight off; do
  check_ok "set_skin:$s" "{\"command\":\"set_skin\",\"args\":{\"skin\":\"$s\"}}"
done
check_ok desktop_color '{"command":"desktop_color","args":{"fg":"7","bg":"0"}}'
check_ok desktop_texture '{"command":"desktop_texture","args":{"char":"░"}}'
check_ok screensaver_off '{"command":"screensaver","args":{"action":"off"}}'
check_ok save_workspace '{"command":"save_workspace","args":{"path":"workspaces/smoke-test.json"}}'
check_ok open_workspace '{"command":"open_workspace","args":{"path":"workspaces/smoke-test.json"}}'
cmd '{"command":"close_all"}' >/dev/null

echo ""
echo "══ smoke: $PASS pass, $FAIL fail ══"
[ -n "$FAILED" ] && echo "failed:$FAILED"
[ "$FAIL" -eq 0 ]
