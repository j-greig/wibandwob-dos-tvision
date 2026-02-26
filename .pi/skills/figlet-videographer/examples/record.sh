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
TMUX_PANE="${TMUX_SESSION}:0.0"

# Use canvas size from timeline meta (not the current terminal size)
CANVAS_W=$(python3 -c "
import json
with open('$TIMELINE') as f: t = json.load(f)
print(t.get('meta',{}).get('canvas',{}).get('width', 152))
")
CANVAS_H=$(python3 -c "
import json
with open('$TIMELINE') as f: t = json.load(f)
print(t.get('meta',{}).get('canvas',{}).get('height', 46))
")
# Add 2 rows for menu/status bar
COLS=$CANVAS_W
ROWS=$((CANVAS_H + 4))

# Save current terminal size to restore later
ORIG_COLS=$(tmux display-message -t "$TMUX_PANE" -p '#{pane_width}' 2>/dev/null || echo "$COLS")
ORIG_ROWS=$(tmux display-message -t "$TMUX_PANE" -p '#{pane_height}' 2>/dev/null || echo "$ROWS")

echo "=== Figlet Videographer Recorder ==="
echo "  Timeline: $(basename "$TIMELINE")"
echo "  Canvas:   ${CANVAS_W}x${CANVAS_H}"
echo "  Record:   ${COLS}x${ROWS} (was ${ORIG_COLS}x${ORIG_ROWS})"
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

# Resize tmux to match canvas
echo "Resizing tmux pane to ${COLS}x${ROWS}..."
tmux resize-window -t "$TMUX_SESSION" -x "$COLS" -y "$ROWS" 2>/dev/null || true
sleep 0.5

# ── Record ──
# play.py drives the TUI via API in the background
# asciinema records a read-only tmux attach (sees the TUI output directly)
# A background sleeper detaches tmux after the performance ends

echo "Clearing canvas..."
# Clear before recording starts so first frame is blank
python3 -c "
import urllib.request, json
body = json.dumps({'command':'close_all'}).encode()
req = urllib.request.Request('http://127.0.0.1:8089/menu/command', body, {'Content-Type':'application/json'})
urllib.request.urlopen(req, timeout=5)
body = json.dumps({'command':'desktop_gallery','args':{'on':'true'}}).encode()
req = urllib.request.Request('http://127.0.0.1:8089/menu/command', body, {'Content-Type':'application/json'})
urllib.request.urlopen(req, timeout=5)
body = json.dumps({'command':'close_all'}).encode()
req = urllib.request.Request('http://127.0.0.1:8089/menu/command', body, {'Content-Type':'application/json'})
urllib.request.urlopen(req, timeout=5)
" 2>/dev/null
sleep 1

echo "Recording... (Ctrl-C to abort)"

# Pre-render speech to wav files for mixing into MP4
SPEECH_DIR=$(mktemp -d /tmp/speech.XXXXXX)
python3 -c "
import json, subprocess, os
with open('$TIMELINE') as f: t = json.load(f)
speech = t.get('meta',{}).get('speech',{})
if not speech.get('enabled'): exit()
default_voice = speech.get('default_voice','Wobble')
default_rate = speech.get('default_rate',130)
manifest = []
for frame in t.get('frames',[]):
    ft = frame.get('t',0)
    delay = frame.get('typewriter_delay',0.5)
    for i, word in enumerate(frame.get('words',[])):
        say_cfg = word.get('say',{})
        if not say_cfg: continue
        voice = say_cfg.get('voice', default_voice)
        rate = say_cfg.get('rate', default_rate)
        t_word = ft + i * delay
        wav = os.path.join('$SPEECH_DIR', f'word_{t_word:.1f}.aiff')
        cmd = ['say', '-v', voice, '-o', wav]
        if rate: cmd += ['-r', str(rate)]
        cmd.append(word['text'])
        subprocess.run(cmd, capture_output=True)
        manifest.append({'t': t_word, 'file': wav})
        print(f'  say {word[\"text\"]:12s} t={t_word:.1f}s voice={voice}')
with open('$SPEECH_DIR/manifest.json','w') as f:
    json.dump(manifest, f)
" 2>/dev/null
echo ""

python3 "$PLAY" "$TIMELINE" --auto-layout --mute > /tmp/play_log.txt 2>&1 &
PLAY_PID=$!

# Auto-detach after duration
(sleep "$DURATION" && tmux detach-client 2>/dev/null) &
DETACH_PID=$!

# Force the recording terminal to match the TUI pane size.
# asciinema's --cols/--rows only sets the cast header;
# the actual view is constrained by the TTY size, which tmux uses
# to decide how much of the pane to show. We use stty to resize
# before attaching so tmux renders the full pane.
asciinema rec "$CAST" \
  --cols "$COLS" --rows "$ROWS" \
  --overwrite \
  --command "stty cols $COLS rows $ROWS; tmux attach-session -t $TMUX_SESSION -r"

# Cleanup (|| true to avoid set -e exit)
kill $PLAY_PID 2>/dev/null || true
wait $PLAY_PID 2>/dev/null || true
kill $DETACH_PID 2>/dev/null || true
wait $DETACH_PID 2>/dev/null || true

echo ""
echo "Cast: $CAST ($(du -h "$CAST" | cut -f1))"
cat /tmp/play_log.txt

# ── GIF ──
if $MAKE_GIF || $MAKE_MP4; then
  echo ""
  echo "Rendering GIF..."
  agg "$CAST" "$GIF" --font-size 28
  echo "GIF: $GIF ($(du -h "$GIF" | cut -f1))"
fi

# ── MP4 with audio ──
if $MAKE_MP4; then
  echo ""
  # Mix speech audio files with backing track
  MIXED_AUDIO="$DIR/$BASE-mixed.wav"
  if [ -f "$SPEECH_DIR/manifest.json" ]; then
    echo "Mixing speech + backing audio..."
    python3 -c "
import json, subprocess, os

with open('$SPEECH_DIR/manifest.json') as f:
    manifest = json.load(f)

backing = '$BACKING'
dur = $DURATION

# Build ffmpeg filter: overlay each speech file at its timestamp
inputs = []
filters = []

if backing and os.path.exists(backing):
    inputs += ['-i', backing]
    base = '[0:a]'
    idx = 1
else:
    # Silent base track
    inputs += ['-f', 'lavfi', '-i', f'anullsrc=r=44100:cl=mono:d={dur}']
    base = '[0:a]'
    idx = 1

for i, entry in enumerate(manifest):
    f = entry['file']
    if not os.path.exists(f): continue
    inputs += ['-i', f]
    filters.append(f'[{idx}:a]adelay={int(entry[\"t\"]*1000)}|{int(entry[\"t\"]*1000)}[s{i}]')
    idx += 1

if filters:
    # Mix all speech streams with the backing
    mix_inputs = base + ''.join(f'[s{i}]' for i in range(len(filters)))
    filters.append(f'{mix_inputs}amix=inputs={len(filters)+1}:duration=longest[out]')
    filter_str = ';'.join(filters)
    cmd = ['ffmpeg', '-y'] + inputs + ['-filter_complex', filter_str, '-map', '[out]', '$MIXED_AUDIO']
else:
    cmd = ['ffmpeg', '-y', '-i', backing, '$MIXED_AUDIO']

subprocess.run(cmd, capture_output=True)
print(f'Mixed audio: $MIXED_AUDIO')
" 2>/dev/null
  fi

  if [ -f "$MIXED_AUDIO" ]; then
    echo "Muxing video + mixed audio..."
    ffmpeg -y -i "$GIF" -i "$MIXED_AUDIO" \
      -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2" \
      -c:v libx264 -pix_fmt yuv420p \
      -c:a aac -shortest \
      -movflags faststart \
      "$MP4" 2>/dev/null
    rm -f "$MIXED_AUDIO"
  elif [ -n "$BACKING" ] && [ -f "$BACKING" ]; then
    echo "Muxing video + backing only..."
    ffmpeg -y -i "$GIF" -i "$BACKING" \
      -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2" \
      -c:v libx264 -pix_fmt yuv420p \
      -c:a aac -shortest \
      -movflags faststart \
      "$MP4" 2>/dev/null
  else
    echo "Converting to MP4 (no audio)..."
    ffmpeg -y -i "$GIF" -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2" \
      -movflags faststart -pix_fmt yuv420p "$MP4" 2>/dev/null
  fi
  rm -rf "$SPEECH_DIR"
  echo "MP4: $MP4 ($(du -h "$MP4" | cut -f1))"
fi

echo ""
echo "Done!"
