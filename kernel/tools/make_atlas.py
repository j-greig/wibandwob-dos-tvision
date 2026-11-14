#!/usr/bin/env python3
# /// script
# dependencies = ["pillow"]
# ///
"""Bake a 1-bit glyph atlas for kernel/pages/shell-webgl.html from GNU Unifont.

usage: make_atlas.py --font unifont-16.0.04.otf [--out kernel/pages/atlas.js] [--extra "chars"] [--png out.png]
       (or set UNIFONT to the .otf path)

The font is not in this repository. Get GNU Unifont from https://unifoundry.com/unifont/ (the .otf
in font/precompiled/ of the release archive). Unifont's fonts are dual-licensed under the GNU GPL 2+
with the GNU font embedding exception and the SIL Open Font License 1.1; the atlas this tool writes
is a bitmap subset of the font and carries the same terms (see THIRD-PARTY.md).

Glyph set: CP437 (Turbo Vision frames, blocks, arrows) + every non-ASCII character in the primer
library (modules/wibwob-2026/primers) and the app's sources (menu text, kaomoji) + --extra. Wide
(16 px) glyphs are stored as two 8x16 halves at consecutive indices; the page draws index g in the
wide cell and g+1 in its trail. A character outside the atlas is drawn as '?'.

Output defines window.WWDOS_ATLAS = {cps: [codepoint,...], idx: [glyph index per cp],
wide: [0|1 per cp], n: glyph count, bits: "<base64 of n*16 bytes, one bit per pixel, MSB left>"}.
"""
import argparse, base64, glob, os, pathlib, sys, unicodedata
from PIL import Image, ImageDraw, ImageFont

KERNEL = pathlib.Path(__file__).resolve().parent.parent
ROOT = KERNEL.parent
CP437 = ("\0☺☻♥♦♣♠•◘○◙♂♀♪♫☼►◄↕‼¶§▬↨↑↓→←∟↔▲▼ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
         "`abcdefghijklmnopqrstuvwxyz{|}~⌂ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒáíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐"
         "└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ")


def gather(extra: str) -> list:
    chars = set(CP437) | set(extra)
    files = (glob.glob(str(ROOT / "modules/wibwob-2026/primers/*.txt"))
             + glob.glob(str(ROOT / "app/**/*.cpp"), recursive=True) + glob.glob(str(ROOT / "app/**/*.h"), recursive=True))
    for f in files:
        chars |= set(pathlib.Path(f).read_text(encoding="utf-8", errors="replace"))
    out = []
    for c in chars:
        o = ord(c)
        if o < 32 or unicodedata.category(c) in ("Mn", "Me", "Cf", "Cc", "Cs"):
            continue  # combining marks and controls have no cell of their own
        out.append(c)
    return sorted(out, key=ord)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--font", default=os.environ.get("UNIFONT", ""), help="GNU Unifont .otf (or $UNIFONT)")
    ap.add_argument("--out", default=str(KERNEL / "pages/atlas.js"))
    ap.add_argument("--extra", default="")
    ap.add_argument("--png", default="")
    a = ap.parse_args()
    if not a.font or not pathlib.Path(a.font).is_file():
        sys.exit("make_atlas.py: give --font PATH to GNU Unifont's .otf (https://unifoundry.com/unifont/)")
    font = ImageFont.truetype(a.font, 16)
    chars = gather(a.extra)
    cps, idx, wide, glyphs = [], [], [], []
    missing = []
    for c in chars:
        w = 16 if font.getlength(c) >= 16 else 8
        im = Image.new("L", (w, 16), 0)
        ImageDraw.Draw(im).text((0, 0), c, font=font, fill=255)
        px = im.load()
        if not any(px[x, y] for x in range(w) for y in range(16)) and c != " ":
            missing.append(c)
        cps.append(ord(c)); idx.append(len(glyphs)); wide.append(1 if w == 16 else 0)
        for half in range(w // 8):
            rows = bytearray()
            for y in range(16):
                b = 0
                for x in range(8):
                    if px[half * 8 + x, y] > 127:
                        b |= 0x80 >> x
                rows.append(b)
            glyphs.append(bytes(rows))
    bits = base64.b64encode(b"".join(glyphs)).decode()
    js = ("/* Glyphs from GNU Unifont (unifoundry.com), GPL-2.0-or-later with the font embedding exception"
          " or OFL-1.1. Made by kernel/tools/make_atlas.py. */\n"
          "window.WWDOS_ATLAS={cps:%s,idx:%s,wide:%s,n:%d,bits:\"%s\"};\n"
          % ("[" + ",".join(map(str, cps)) + "]", "[" + ",".join(map(str, idx)) + "]",
             "[" + ",".join(map(str, wide)) + "]", len(glyphs), bits))
    pathlib.Path(a.out).write_text(js)
    print(f"{len(chars)} chars, {len(glyphs)} glyph cells, {len(js):,} bytes js; blank glyphs: {''.join(missing) or 'none'}")
    if a.png:
        cols = 32
        rows_n = (len(glyphs) + cols - 1) // cols
        sheet = Image.new("L", (cols * 8, rows_n * 16), 0)
        for g, rows in enumerate(glyphs):
            for y, b in enumerate(rows):
                for x in range(8):
                    if b & (0x80 >> x):
                        sheet.putpixel(((g % cols) * 8 + x, (g // cols) * 16 + y), 255)
        sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(a.png)
        print("sheet:", a.png)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
