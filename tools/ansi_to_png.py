#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow>=10.4.0"]
# ///
"""Render a tmux pane (or an ANSI text file) to PNG, colours included, and count its colours.

    uv run tools/ansi_to_png.py --tmux dosbuild -o shot.png        # capture-pane -e -N of that session
    uv run tools/ansi_to_png.py screen.ans -o shot.png             # a saved ANSI capture
    uv run tools/ansi_to_png.py --tmux dosbuild --ega-check        # exit 3 if any colour is outside the 16 EGA colours

Cells are 8x16 (Unifont). Handles SGR 0/1/7/22/27/39/49, 30-37/90-97/40-47/100-107, 38;5;n/48;5;n and
38;2;r;g;b/48;2;r;g;b. Wide characters take two cells. Prints the colour histogram (fg and bg) when asked.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import unicodedata
from collections import Counter
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_CANDIDATES = [
    Path.home() / "Repos/wibandwob-heartbeat/.claude/skills/tweet/scripts/fonts/unifont-mono.ttf",
    Path.home() / "Repos/wibandwob-heartbeat/fonts/UnifontExMono.ttf",
]
# The 16 EGA colours as Turbo Vision's 24-bit backend draws them (xterm-style index order).
EGA = [
    (0x00, 0x00, 0x00), (0x00, 0x00, 0xAA), (0x00, 0xAA, 0x00), (0x00, 0xAA, 0xAA),
    (0xAA, 0x00, 0x00), (0xAA, 0x00, 0xAA), (0xAA, 0x55, 0x00), (0xAA, 0xAA, 0xAA),
    (0x55, 0x55, 0x55), (0x55, 0x55, 0xFF), (0x55, 0xFF, 0x55), (0x55, 0xFF, 0xFF),
    (0xFF, 0x55, 0x55), (0xFF, 0x55, 0xFF), (0xFF, 0xFF, 0x55), (0xFF, 0xFF, 0xFF),
]
# SGR 30-37 order is black red green yellow blue magenta cyan white; map to the EGA (BIOS) order.
ANSI_TO_EGA = [0, 4, 2, 6, 1, 5, 3, 7]
DEF_FG, DEF_BG = (0xAA, 0xAA, 0xAA), (0x00, 0x00, 0x00)
SGR = re.compile(r"\x1b\[([0-9;:]*)m")
OTHER_ESC = re.compile(r"\x1b(\[[0-9;?]*[A-Za-ln-z]|\][^\x07]*\x07|[()][A-Z0-9])")


def xterm256(n: int):
    if n < 16:
        # xterm order: 0 black 1 red 2 green 3 yellow 4 blue 5 magenta 6 cyan 7 white, 8-15 bright
        base = ANSI_TO_EGA[n % 8] + (8 if n >= 8 else 0)
        return EGA[base]
    if n < 232:
        n -= 16
        lv = [0, 95, 135, 175, 215, 255]
        return (lv[n // 36], lv[(n // 6) % 6], lv[n % 6])
    v = 8 + (n - 232) * 10
    return (v, v, v)


def cw(ch: str) -> int:
    return 2 if unicodedata.east_asian_width(ch) in ("W", "F") else 1


def parse(text: str):
    """ANSI text -> list of rows, each a list of (char, fg, bg) cells."""
    rows = []
    fg, bg, bold, rev = DEF_FG, DEF_BG, False, False
    for line in text.split("\n"):
        row = []
        pos = 0
        line = OTHER_ESC.sub("", line)
        for m in list(SGR.finditer(line)) + [None]:
            end = m.start() if m else len(line)
            for ch in line[pos:end]:
                if ch == "\r":
                    continue
                f, b = (bg, fg) if rev else (fg, bg)
                row.append((ch, f, b))
                if cw(ch) == 2:
                    row.append(("", f, b))
            if not m:
                break
            pos = m.end()
            params = [p for p in re.split("[;:]", m.group(1))] or ["0"]
            i = 0
            while i < len(params):
                p = int(params[i] or 0)
                if p == 0:
                    fg, bg, bold, rev = DEF_FG, DEF_BG, False, False
                elif p == 1:
                    bold = True
                elif p == 22:
                    bold = False
                elif p == 7:
                    rev = True
                elif p == 27:
                    rev = False
                elif 30 <= p <= 37:
                    fg = EGA[ANSI_TO_EGA[p - 30]]
                elif 90 <= p <= 97:
                    fg = EGA[ANSI_TO_EGA[p - 90] + 8]
                elif 40 <= p <= 47:
                    bg = EGA[ANSI_TO_EGA[p - 40]]
                elif 100 <= p <= 107:
                    bg = EGA[ANSI_TO_EGA[p - 100] + 8]
                elif p == 39:
                    fg = DEF_FG
                elif p == 49:
                    bg = DEF_BG
                elif p in (38, 48) and i + 1 < len(params):
                    mode = int(params[i + 1] or 0)
                    if mode == 5 and i + 2 < len(params):
                        c = xterm256(int(params[i + 2] or 0))
                        i += 2
                    elif mode == 2 and i + 4 < len(params):
                        c = tuple(int(params[i + k] or 0) for k in (2, 3, 4))
                        i += 4
                    else:
                        c = None
                    if c is not None:
                        if p == 38:
                            fg = c
                        else:
                            bg = c
                i += 1
        rows.append(row)
    while rows and not rows[-1]:
        rows.pop()
    return rows


def render(rows, out: Path, cell=(8, 16)):
    font_path = next((p for p in FONT_CANDIDATES if p.exists()), None)
    font = ImageFont.truetype(str(font_path), 16) if font_path else ImageFont.load_default()
    cols = max((len(r) for r in rows), default=1)
    W, H = cols * cell[0], len(rows) * cell[1]
    img = Image.new("RGB", (W, H), DEF_BG)
    d = ImageDraw.Draw(img)
    for y, row in enumerate(rows):
        for x, (ch, f, b) in enumerate(row):
            if ch == "":
                continue
            w = cw(ch)
            x0, y0 = x * cell[0], y * cell[1]
            d.rectangle([x0, y0, x0 + cell[0] * w - 1, y0 + cell[1] - 1], fill=b)
            if ch != " ":
                d.text((x0, y0), ch, font=font, fill=f)
    img.save(out)
    return W, H


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("file", nargs="?", help="ANSI text file (default: read --tmux)")
    ap.add_argument("--tmux", help="tmux session/pane to capture with capture-pane -e")
    ap.add_argument("-o", "--out", help="PNG path")
    ap.add_argument("--ega-check", action="store_true", help="exit 3 if a colour outside the 16 EGA colours is drawn")
    ap.add_argument("--colours", action="store_true", help="print the colour histogram")
    a = ap.parse_args()
    if a.tmux:
        # -N keeps trailing spaces: without it tmux drops a line's trailing coloured blanks
        # (a status line that ends in grey spaces comes back black).
        text = subprocess.run(["tmux", "capture-pane", "-e", "-p", "-N", "-t", a.tmux],
                              capture_output=True, text=True, check=True).stdout
    elif a.file:
        text = Path(a.file).read_text(encoding="utf-8", errors="replace")
    else:
        text = sys.stdin.read()
    rows = parse(text)
    if a.out:
        W, H = render(rows, Path(a.out))
        print(f"wrote {a.out} {W}x{H}")
    hist = Counter()
    for row in rows:
        for ch, f, b in row:
            if ch == "":
                continue
            hist[("bg", b)] += 1
            if ch.strip():
                hist[("fg", f)] += 1
    off = {k: v for k, v in hist.items() if k[1] not in EGA}
    if a.colours or a.ega_check:
        for (kind, c), n in hist.most_common():
            tag = "" if c in EGA else "  NOT-EGA"
            name = EGA.index(c) if c in EGA else "#%02x%02x%02x" % c
            print(f"{kind} {name!s:>8} {n:6d}{tag}")
    if a.ega_check:
        cells = sum(off.values())
        print(f"ega-check: {len(off)} non-EGA colours on {cells} cells")
        return 3 if off else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
