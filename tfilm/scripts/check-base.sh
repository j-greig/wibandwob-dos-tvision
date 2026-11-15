#!/usr/bin/env bash
# Check that tfilm/base/film.html is the page a fresh build makes: tfilm/scripts/check-base.sh [--no-build]
# Builds with kernel/build-web.sh (needs emsdk; JOBS sets the parallel jobs) unless --no-build is given, then
# compares dist-web/film.html with tfilm/base/film.html byte for byte.
# Exit 0: identical. 1: they differ. 3: no build (emcc missing, or --no-build with no dist-web/film.html).
set -euo pipefail
cd "$(dirname "$0")/../.."
if [ "${1:-}" != "--no-build" ]; then
    log=$(mktemp)
    kernel/build-web.sh >"$log" 2>&1 || { rc=$?; tail -20 "$log"; rm -f "$log"; exit "$rc"; }
    rm -f "$log"
fi
[ -f dist-web/film.html ] || { echo "no dist-web/film.html: run kernel/build-web.sh"; exit 3; }
sum() { shasum -a 256 "$1" | cut -d' ' -f1; }
if cmp -s dist-web/film.html tfilm/base/film.html; then
    echo "identical: tfilm/base/film.html $(sum tfilm/base/film.html)"
    exit 0
fi
echo "differ: tfilm/base/film.html $(sum tfilm/base/film.html), dist-web/film.html $(sum dist-web/film.html)"
cmp dist-web/film.html tfilm/base/film.html | head -1 || true
echo "if the change is intended: cp dist-web/film.html tfilm/base/film.html and commit it with the engine change"
exit 1
