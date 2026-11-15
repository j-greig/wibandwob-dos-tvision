#!/usr/bin/env bash
# Build the browser pages: kernel/build-web.sh [--clean]
# Output: build-web/ (wasm, js) and dist-web/*.html (one self-contained file per page).
# JOBS=N limits the parallel compile jobs (default: one per core).
# Needs emsdk (https://emscripten.org/docs/getting_started/downloads.html); $EMSDK or ~/emsdk is sourced if emcc is not on PATH.
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v emcc >/dev/null 2>&1; then
    env="${EMSDK:-$HOME/emsdk}/emsdk_env.sh"
    [ -f "$env" ] && source "$env" >/dev/null 2>&1
fi
command -v emcc >/dev/null || { echo "emcc not found: install emsdk (see docs/web-port.md)"; exit 3; }
[ "${1:-}" = "--clean" ] && rm -rf build-web
emcmake cmake -B build-web -S kernel -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-web -j"${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc)}"
mkdir -p dist-web
for t in hello wwdos; do
    [ -f "build-web/$t.js" ] || continue
    python3 kernel/tools/bundle.py "build-web/$t.js" "dist-web/$t.html"
    python3 kernel/tools/bundle.py "build-web/$t.js" "dist-web/$t-webgl.html" --shell kernel/pages/shell-webgl.html
done
# The film player has its own page: the WebGL renderer plus the stage's clock, sound, gate and scrubber.
if [ -f build-web/film.js ]; then
    python3 kernel/tools/bundle.py build-web/film.js dist-web/film.html --shell kernel/pages/shell-film.html --atlas kernel/pages/atlas-film.js
fi
ls -l dist-web/*.html
