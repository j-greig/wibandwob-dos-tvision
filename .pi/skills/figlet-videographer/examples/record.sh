#!/bin/bash
# Record a figlet videographer performance via asciinema
#
# Usage: ./record.sh <timeline.json> [--gif] [--mp4]
#
# ⚠️  Run this from a REAL terminal (not from pi/codex) — asciinema needs a TTY.
#
# Pipeline:
#   1. asciinema records a read-only tmux attach (sees the TUI directly)
#   2. play.py runs in background driving the TUI via API
#   3. agg renders .cast → .gif
#   4. ffmpeg muxes .gif + backing track → .mp4
#
# The TUI must be running in tmux session "wwdos".
# Requirements: asciinema, agg, ffmpeg, python3, tmux

set -euo pipefail

TIMELINE="$(cd "$(dirname "${1:?Usage: record.sh <timeline.json> [--gif] [--mp4]}")" && pwd)/$(basename "$1")"
MAKE_GIF=false; MAKE_MP4=false
for arg in "$@"; do
  case "$arg" in --gif) MAKE_GIF=true ;; --mp4) MAKE_MP4=true ;; esac
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
DURATION=$(python3 -c "
import json
with open('$TIMELINE') as f: t = json.load(f)
frames = t.get('frames',[])
print(int(max((f.get('t',0) for f in frames), default=0)) + 12)
")

TMUX_SESSION="wwdos"
COLS=$(tmux display-message -t "${TMUX_SESSION}:0.0" -p '#{pane_width}' 2>/dev/null || echo 152)
ROWS=$(tmux display-message -t "${TMUX_SESSION}:0.0" -p '#{pane_height}' 2>/dev/null || echo 46)

echo "=== Figlet Videographer Recorder ==="
echo "  Timeline: $(basename "$TIMELINE")"
echo "  Terminal: ${COLS}x${ROWS}"
echo "  Duration: ${DURATION}s"
[ -n "$BACKING" ] && echo "  Audio:    $(basename "$BACKING")"
echo ""

# Check we have a TTY
if [ ! -t 0 ]; then
  echo "ERROR: No TTY — run this from a real terminal, not from pi/codex."
  echo ""
  echo "  cd $(pwd)"
  echo "  bash $0 $*"
  exit 1
fi

# ── Record ──
# play.py drives the TUI via API in the background
# asciinema records a read-only tmux attach (sees the TUI output directly)
# A background sleeper detaches tmux after the performance ends

echo "Recording... (Ctrl-C to abort)"

python3 "$PLAY" "$TIMELINE" --auto-layout > /tmp/play_log.txt 2>&1 &
PLAY_PID=$!

# Auto-detach after duration
(sleep "$DURATION" && tmux detach-client 2>/dev/null) &
DETACH_PID=$!

asciinema rec "$CAST" \
  --cols "$COLS" --rows "$ROWS" \
  --overwrite \
  --command "tmux attach-session -t $TMUX_SESSION -r"

# Cleanup
kill $PLAY_PID 2>/dev/null; wait $PLAY_PID 2>/dev/null || true
kill $DETACH_PID 2>/dev/null; wait $DETACH_PID 2>/dev/null || true

echo ""
echo "Cast: $CAST ($(du -h "$CAST" | cut -f1))"
cat /tmp/play_log.txt

# ── GIF ──
if $MAKE_GIF || $MAKE_MP4; then
  echo ""
  echo "Rendering GIF..."
  agg "$CAST" "$GIF" --font-size 14
  echo "GIF: $GIF ($(du -h "$GIF" | cut -f1))"
fi

# ── MP4 with audio ──
if $MAKE_MP4; then
  echo ""
  if [ -n "$BACKING" ] && [ -f "$BACKING" ]; then
    echo "Muxing with audio..."
    ffmpeg -y -i "$GIF" -i "$BACKING" \
      -c:v libx264 -pix_fmt yuv420p \
      -c:a aac -shortest \
      -movflags faststart \
      "$MP4" 2>/dev/null
  else
    echo "Converting to MP4 (no audio)..."
    ffmpeg -y -i "$GIF" -movflags faststart -pix_fmt yuv420p "$MP4" 2>/dev/null
  fi
  echo "MP4: $MP4 ($(du -h "$MP4" | cut -f1))"
fi

echo ""
echo "Done!"
