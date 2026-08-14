#!/bin/bash
# skin_scene.sh — dense CGA-skinned desktop scene, D-Flat-ref density.
# Usage: ./scripts/skin_scene.sh [dflat|turbo|terra|pipeline]  (default dflat)
#
# The Figma refs (design/figma-refs/) work because they're CROWDED: 3+
# overlapping windows, dialogs floating over paper over a dithered sea.
# Two windows on a desktop is a haiku; this script throws the pub argument.
#
# Requires: wwdos running (scripts/launch_wwdos_ghostty.sh via Ghostty
# AppleScript) + API server on :8089. Skins colour primer/text windows only
# (frame_player / text views) — generative app windows keep native palettes.

set -u
SKIN="${1:-dflat}"
API="http://localhost:8089/menu/command"
REPO="$(cd "$(dirname "$0")/.." && pwd)"

cmd() { curl -s -X POST "$API" -H 'Content-Type: application/json' -d "$1"; echo; }

primer() { # primer NTH-FILE X Y W H
  local p
  p=$(ls "$REPO"/modules/wibwob-primers/primers/*.txt | sed -n "${1}p")
  cmd "{\"command\":\"open_primer\",\"args\":{\"path\":\"$p\",\"x\":\"$2\",\"y\":\"$3\",\"w\":\"$4\",\"h\":\"$5\"}}"
}

# Layered cascade, back-to-front (later opens sit on top). Rects scale to
# the live canvas so the scene fills tall desktops instead of clustering top.
read CW CH <<EOF
$(curl -s http://localhost:8089/state | python3 -c "
import json,sys; d=json.load(sys.stdin)['canvas']; print(d['width'], d['height'])")
EOF
CW=${CW:-200}; CH=${CH:-60}
Y2=$((CH/3)); Y3=$((CH/2)); Y4=$((CH*3/5))
primer 1   2  2         74 $((CH/3+4))
primer 8  30  $((CH/6)) 64 $((CH/3))
primer 15 $((CW/2+8)) 3 76 $((CH/3+6))
primer 22 $((CW*2/3)) $Y2 62 $((CH/3+2))
primer 30  18 $Y3       70 $((CH/3))
primer 12 $((CW/3)) $Y4 58 $((CH/3-2))
primer 18  $((CW/2-20)) $((CH/4)) 66 $((CH/3))

sleep 1
# One-shot skin: chrome + desktop + paper on all primer windows
cmd "{\"command\":\"set_skin\",\"args\":{\"skin\":\"$SKIN\"}}"
sleep 1

# Verify the skin actually landed (burst-race guard), resend once if not
GOT=$(curl -s http://localhost:8089/state | python3 -c "import json,sys; print(json.load(sys.stdin).get('skin',''))")
if [ "$GOT" != "$SKIN" ]; then
  cmd "{\"command\":\"set_skin\",\"args\":{\"skin\":\"$SKIN\"}}"
fi

# Dialog accents: pull two mid-stack windows out of the paper (blue dialog in
# dflat, per-skin accent otherwise). IDs queried live so this survives reruns.
IDS=$(curl -s http://localhost:8089/state | python3 -c "
import json,sys
ws = sorted(json.load(sys.stdin)['windows'], key=lambda w: w['id'])
print(' '.join(w['id'] for w in ws))")
set -- $IDS
case "$SKIN" in
  dflat)    DBG=1;  DFG=15 ;;
  turbo)    DBG=7;  DFG=0  ;;
  terra)    DBG=2;  DFG=0  ;;
  pipeline) DBG=0;  DFG=13 ;;
esac
N=0
for id in $IDS; do
  N=$((N+1))
  if [ $N -eq 2 ] || [ $N -eq 5 ]; then
    cmd "{\"command\":\"set_window_bg\",\"args\":{\"id\":\"$id\",\"idx\":\"$DBG\"}}"
    cmd "{\"command\":\"set_window_fg\",\"args\":{\"id\":\"$id\",\"idx\":\"$DFG\"}}"
  fi
done

echo "scene: $SKIN applied ($(echo $IDS | wc -w | tr -d ' ') windows)"
echo "NOTE: activate the Ghostty window before screenshotting — macOS naps"
echo "backgrounded terminals and screencapture returns the stale backing store."
