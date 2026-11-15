#!/bin/sh
# The residents answer you in the same room: open or close a window, or type in an editor, and Wib,
# Wob or Scramble writes back in the "residents" window with a visible hand (W, W, S); the Feed shows
# every act with who made it. No model is called (the claude CLI is kept off PATH).
#   scripts/resident-world-demo.sh            # runs in this terminal
#   WWDOS_BIN=<binary> scripts/resident-world-demo.sh
cd "$(dirname "$0")/.." || exit 1
BIN=${WWDOS_BIN:-build-resident/app/wwdos}
[ -x "$BIN" ] || { echo "no binary at $BIN: cmake -B build-resident -S . && cmake --build build-resident --target wwdos -j4"; exit 1; }
exec env PATH=/usr/bin:/bin:/usr/sbin:/sbin ANTHROPIC_API_KEY= WIBWOB_INSTANCE=demo WWDOS_TR808_AUDIO=0 \
    WWDOS_RESIDENTS=residents/demo.reflex "$BIN" 2>/tmp/wibwob_demo.stderr
