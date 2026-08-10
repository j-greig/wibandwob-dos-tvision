#!/usr/bin/env bash
# vidframe.sh — extract ONE full-res frame from a video at time T (seconds).
# Usage: vidframe.sh <video.mp4> <seconds> [out.png]
# Default out: <video>_<seconds>s.png next to the video.
set -euo pipefail
V="${1:?video path}"; T="${2:?seconds}"; OUT="${3:-${V%.*}_${T}s.png}"
ffmpeg -nostdin -loglevel error -ss "$T" -i "$V" -frames:v 1 "$OUT"
echo "$OUT"
