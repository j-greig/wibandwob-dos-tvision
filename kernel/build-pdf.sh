#!/usr/bin/env bash
# Build WibWob-DOS as a PDF: kernel/build-pdf.sh [--clean] [make_pdf.py options, e.g. --no-mouse]
# Output: build-pdf/ (asm.js) and dist-web/wwdos.pdf, dist-web/hello.pdf.
# Needs emsdk (6.0.13 is the tested version, see docs/web-port.md) and fontTools (pip install fonttools).
# Linking the app as asm.js takes about two minutes. Docs: docs/pdf-port.md.
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v emcc >/dev/null 2>&1; then
    env="${EMSDK:-$HOME/emsdk}/emsdk_env.sh"
    [ -f "$env" ] && source "$env" >/dev/null 2>&1
fi
command -v emcc >/dev/null || { echo "emcc not found: install emsdk (see docs/web-port.md)"; exit 3; }
python3 -c 'import fontTools' 2>/dev/null || { echo "fontTools not found: pip install fonttools"; exit 3; }
if [ "${1:-}" = "--clean" ]; then rm -rf build-pdf; shift; fi
emcmake cmake -B build-pdf -S kernel -DCMAKE_BUILD_TYPE=Release -DWWDOS_PDF=ON -DWWDOS_WEB_FILM=OFF -DWWDOS_SINGLE_FILE=OFF >/dev/null
cmake --build build-pdf -j"${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc)}"
mkdir -p dist-web
for t in wwdos hello; do
    [ -f "build-pdf/$t.js" ] || continue
    python3 kernel/tools/make_pdf.py "build-pdf/$t.js" "dist-web/$t.pdf" "$@"
done
ls -l dist-web/*.pdf
