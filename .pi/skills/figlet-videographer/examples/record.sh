#!/bin/bash
# Record a figlet videographer performance as .cast / .gif / .mp4
#
# Usage: ./record.sh <timeline.json> [--gif] [--mp4] [--mute]
#
# Pipeline:
#   1. Capture TUI pane with tmux capture-pane in a polling loop → .cast
#   2. play.py drives the TUI via API (audio plays live)
#   3. agg renders .cast → .gif
#   4. ffmpeg muxes .gif + backing track → .mp4
#
# The TUI must be running in tmux session "wwdos".
# Requirements: agg, ffmpeg, python3, tmux

set -euo pipefail

TIMELINE="$(cd "$(dirname "${1:?Usage: record.sh <timeline.json> [--gif] [--mp4] [--mute]}")" && pwd)/$(basename "$1")"
MAKE_GIF=false; MAKE_MP4=false; MUTE=""
for arg in "$@"; do
  case "$arg" in
    --gif)  MAKE_GIF=true ;;
    --mp4)  MAKE_MP4=true ;;
    --mute) MUTE="--mute" ;;
  esac
done
$MAKE_GIF || $MAKE_MP4 || MAKE_GIF=true

DIR="$(dirname "$TIMELINE")"
BASE="$(basename "$TIMELINE" .json)"
CAST="$DIR/$BASE.cast"
GIF="$DIR/$BASE.gif"
MP4="$DIR/$BASE.mp4"
PLAY="$DIR/play.py"

BACKING=$(python3 -c "
import json
with open('$TIMELINE') as f: t = json.load(f)
print(t.get('meta',{}).get('audio',{}).get('backing',''))
")

TMUX_PANE="wwdos:0.0"
COLS=$(tmux display-message -t "$TMUX_PANE" -p '#{pane_width}')
ROWS=$(tmux display-message -t "$TMUX_PANE" -p '#{pane_height}')

echo "=== Figlet Videographer Recorder ==="
echo "  Timeline: $(basename "$TIMELINE")"
echo "  Terminal: ${COLS}x${ROWS}"
[ -n "$BACKING" ] && echo "  Audio:    $(basename "$BACKING")"
echo ""

# ── Capture loop: poll tmux pane → asciicast v2 format ──
capture_frames() {
  local cast_file="$1" cols="$2" rows="$3" fps="${4:-10}"
  local interval=$(python3 -c "print(1.0/$fps)")

  # Write asciicast v2 header
  echo "{\"version\":2,\"width\":$cols,\"height\":$rows,\"timestamp\":$(date +%s),\"env\":{\"TERM\":\"xterm-256color\"}}" > "$cast_file"

  local start=$(python3 -c "import time; print(time.time())")
  while [ -f "$cast_file.recording" ]; do
    local now=$(python3 -c "import time; print(time.time())")
    local t=$(python3 -c "print(round($now - $start, 3))")
    # Capture full pane content with escape sequences
    local content
    content=$(tmux capture-pane -t "$TMUX_PANE" -p -e 2>/dev/null | python3 -c "
import sys, json
lines = sys.stdin.read()
# Asciicast v2: each frame is [time, 'o', data]
print(json.dumps([float('$t'), 'o', lines]))" 2>/dev/null) || true
    [ -n "$content" ] && echo "$content" >> "$cast_file"
    sleep "$interval"
  done
}

# ── Record ──
echo "Recording..."
touch "$CAST.recording"
capture_frames "$CAST" "$COLS" "$ROWS" 10 &
CAPTURE_PID=$!

# Run the performance
python3 "$PLAY" "$TIMELINE" --auto-layout $MUTE

# Let final frame settle
sleep 2
rm -f "$CAST.recording"
wait $CAPTURE_PID 2>/dev/null || true

FRAMES=$(wc -l < "$CAST")
echo "Cast saved: $CAST ($((FRAMES-1)) frames, $(du -h "$CAST" | cut -f1))"

# ── GIF ──
if $MAKE_GIF || $MAKE_MP4; then
  echo "Rendering GIF..."
  agg "$CAST" "$GIF" \
    --cols "$COLS" --rows "$ROWS" \
    --font-size 14
  echo "GIF: $GIF ($(du -h "$GIF" | cut -f1))"
fi

# ── MP4 with audio ──
if $MAKE_MP4; then
  if [ -n "$BACKING" ] && [ -f "$BACKING" ]; then
    echo "Muxing with audio..."
    ffmpeg -y -i "$GIF" -i "$BACKING" \
      -c:v libx264 -pix_fmt yuv420p \
      -c:a aac -shortest \
      -movflags faststart \
      "$MP4" 2>/dev/null
  else
    ffmpeg -y -i "$GIF" -movflags faststart -pix_fmt yuv420p "$MP4" 2>/dev/null
  fi
  echo "MP4: $MP4 ($(du -h "$MP4" | cut -f1))"
fi

echo "Done!"
