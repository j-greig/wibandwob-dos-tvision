#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow"]
# ///
"""Bake a 1-bit glyph atlas for the WebGL pages (kernel/pages/shell-webgl.html, kernel/pages/shell-film.html) from GNU Unifont.

usage: make_atlas.py [--font FILE.otf] [--out kernel/pages/atlas.js] [--extra "chars"] [--png out.png]
                                                         the desktop atlas (shell-webgl.html)
       make_atlas.py --film [--film-root DIR] [--out kernel/pages/atlas-film.js] [--extra "chars"]
                                                         the film atlas (shell-film.html, the film base page)
       make_atlas.py --chars-json SET.json --out FILE.js     bake exactly the set in SET.json ({char: source})
       make_atlas.py --widths                                 width report for the canon kaomoji

The font is not in this repository. Get GNU Unifont 16.0.04 from https://unifoundry.com/unifont/ (the .otf in
font/precompiled/ of the release archive); --font, $UNIFONT, or ~/Downloads/unifont-16.0.04.otf. Unifont's fonts are
dual-licensed under the GNU GPL 2+ with the GNU font embedding exception and the SIL Open Font License 1.1; the atlas
this tool writes is a bitmap subset of the font and carries the same terms (see THIRD-PARTY.md).

Desktop set: CP437 (Turbo Vision frames, blocks, arrows) + every non-ASCII character in the primer library
(modules/wibwob-2026/primers) and the app's sources (menu text, kaomoji) + --extra. A blank glyph is reported, not
refused.

Film set (--film), the same bytes as the on-chain repository's tools/make_atlas.py writes to web/atlas.js: the
interface set (below) + every character in modules/wibwob-2026/primers and, under the film root (default tfilm/),
stages/primers, stages/*.stage, scenes/*.json, and --extra. The film tools bake a smaller set per page instead
(interface_chars() + what that page ships) and replace the shared atlas in the page.

The interface set is what the film desktop can draw on any page: printable ASCII, the CP437 characters Turbo
Vision draws (frames, scroll bars, icons, shadows: char constants in vendor/tvision/source/tvision/tvtext1.cpp),
the characters in the film's sources (menu bar kaomoji, the 0xB1 background, stage widgets' blocks and bubble
frames), the text kernel/pages/shell-film.html writes into the grid itself, and every character of app/film/boot,
which the File menu can open on any page.

The film build fails, naming each character and where it came from, when the font has no glyph for it: the page
would draw it as "?". Wide (16 px) glyphs are stored as two 8x16 halves at consecutive indices; the page draws
index g in the wide cell and g+1 in its trail. Turbo Vision places the trail cell by wcwidth() (emscripten's
musl); a character whose font advance disagrees with that is reported, since it shifts or cuts a face by a cell.

Output defines window.WWDOS_ATLAS = {cps: [codepoint,...], idx: [glyph index per cp], wide: [0|1 per cp],
n: glyph count, bits: "<base64 of n*16 bytes, one bit per pixel, MSB left>"}.
"""
import argparse, base64, glob, json, os, pathlib, re, sys, unicodedata

KERNEL = pathlib.Path(__file__).resolve().parent.parent
ROOT = KERNEL.parent
FILM_ROOT = ROOT / "tfilm"   # the film sources (stages/, scenes/); a run can name another with --film-root
PRIMERS = "modules/wibwob-2026/primers"
CP437 = ("\0☺☻♥♦♣♠•◘○◙♂♀♪♫☼►◄↕‼¶§▬↨↑↓→←∟↔▲▼ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
         "`abcdefghijklmnopqrstuvwxyz{|}~⌂ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒáíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐"
         "└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ")
# Sources compiled into the film target that put characters on screen (app/film/CMakeLists.txt FILM_SOURCES).
ENGINE_SOURCES = ["app/film/minimal_app.cpp", "app/stage/stage.cpp", "app/stage/mirror_view.cpp",
                  "app/windows/frame_animation_window.cpp", "app/frame_file_player_view.cpp",
                  "app/gradient.cpp", "app/wibwob_background.cpp"]
TV_CHARS = "vendor/tvision/source/tvision/tvtext1.cpp"
SHELL = "kernel/pages/shell-film.html"  # writes its own text into the grid (the "click for sound" note in the menu bar)
BOOT = "app/film/boot/*.txt"


class AtlasError(Exception):
    pass


def drawable(c: str) -> bool:
    return ord(c) >= 32 and unicodedata.category(c) not in ("Mn", "Me", "Cf", "Cc", "Cs")


def add(out: dict, text: str, source: str) -> None:
    """Record each drawable character of text with the first source it came from."""
    for c in text:
        if c not in out and drawable(c):
            out[c] = source


def c_string_chars(src: str) -> str:
    """Characters a C++ source draws through escapes: a run of \\xNN / \\NNN escapes that decodes as UTF-8 is
    taken as UTF-8 (stage.cpp's bubble frame), otherwise each byte is a CP437 code (Turbo Vision's char tables)."""
    out = []
    for run in re.findall(r"(?:\\x[0-9A-Fa-f]{2}|\\[0-7]{3})+", src):
        bs = bytes(int(e[2:], 16) if e[1] == "x" else int(e[1:], 8) for e in re.findall(r"\\x[0-9A-Fa-f]{2}|\\[0-7]{3}", run))
        try:
            s = bs.decode("utf-8")
            if any(ord(ch) > 127 for ch in s):
                out.append(s)
                continue
        except UnicodeDecodeError:
            pass
        out.append("".join(CP437[b] for b in bs if b >= 0x10))  # below 0x10: colour bytes and frame bit masks
    return "".join(out)


def interface_chars() -> dict:
    out = {}
    add(out, "".join(chr(i) for i in range(32, 127)), "printable ASCII")
    tv = (ROOT / TV_CHARS).read_text(encoding="latin-1")
    add(out, c_string_chars(tv), TV_CHARS)
    for f in ENGINE_SOURCES:
        text = (ROOT / f).read_text(encoding="utf-8")
        add(out, text, f)
        add(out, c_string_chars(text), f + " (escapes)")
    shell = (ROOT / SHELL).read_text(encoding="utf-8")
    add(out, "".join(chr(int(h, 16)) for h in re.findall(r"\\u([0-9a-fA-F]{4})", shell)), SHELL + " (escapes)")
    add(out, "".join(c for c in shell if ord(c) > 127), SHELL)
    for f in sorted(glob.glob(str(ROOT / BOOT))):
        add(out, pathlib.Path(f).read_text(encoding="utf-8"), os.path.relpath(f, ROOT))
    return out


def film_sources(film_root: pathlib.Path = FILM_ROOT) -> list:
    return sorted(glob.glob(str(film_root / "stages/primers/*.txt")) + glob.glob(str(film_root / "stages/*.stage"))
                  + glob.glob(str(film_root / "scenes/*.json")))


def shared_chars(extra: str = "", film_root: pathlib.Path = FILM_ROOT) -> dict:
    out = interface_chars()
    files = sorted(glob.glob(str(ROOT / PRIMERS / "*.txt")) + film_sources(film_root))
    for f in files:
        add(out, pathlib.Path(f).read_text(encoding="utf-8", errors="replace"), os.path.relpath(f, ROOT))
    add(out, extra, "--extra")
    return out


def default_font() -> str:
    if os.environ.get("UNIFONT"):
        return os.environ["UNIFONT"]
    home = pathlib.Path.home()
    for p in (home / "Downloads/unifont-16.0.04.otf", home / "Downloads/unifont-16.0.04/font/precompiled/unifont-16.0.04.otf"):
        if p.exists():
            return str(p)
    return str(home / "Downloads/unifont-16.0.04.otf")


# ---- Turbo Vision's width rule: wcwidth() from emscripten's musl (vendor/tvision source/platform/platfchr.cpp unixCharWidth) ----
_MUSL = None


def _musl_tables():
    global _MUSL
    if _MUSL is None:
        base = pathlib.Path(os.environ.get("EMSDK", pathlib.Path.home() / "emsdk")) / "upstream/emscripten/system/lib/libc/musl/src/ctype"
        if not (base / "wide.h").exists():
            _MUSL = False
        else:
            nums = lambda n: [int(x) for x in re.findall(r"\d+", (base / n).read_text())]
            _MUSL = (nums("nonspacing.h"), nums("wide.h"))
    return _MUSL


def tv_width(c: str):
    """Cells Turbo Vision gives c in the wasm build, or None when the musl tables are not on this machine."""
    t = _musl_tables()
    if not t:
        return None
    table, wtable = t
    wc = ord(c)
    if wc < 0xff:
        return 1 if ((wc + 1) & 0x7f) >= 0x21 else (-1 if wc else 0)
    if (wc & 0xfffeffff) < 0xfffe:
        if (table[table[wc >> 8] * 32 + ((wc & 255) >> 3)] >> (wc & 7)) & 1:
            return 0
        if (wtable[wtable[wc >> 8] * 32 + ((wc & 255) >> 3)] >> (wc & 7)) & 1:
            return 2
        return 1
    if (wc & 0xfffe) == 0xfffe:
        return -1
    if wc - 0x20000 < 0x20000:
        return 2
    if wc == 0xe0001 or wc - 0xe0020 < 0x5f or wc - 0xe0100 < 0xef:
        return 0
    return 1


def py_width(c: str) -> int:
    return 2 if unicodedata.east_asian_width(c) in "WF" else 1


# ---- baking ----
def bake(chars: dict, font_path: str = "", optional: set = frozenset()) -> tuple:
    """Return (atlas js, report dict) for exactly these characters. Raises AtlasError for characters the font
    has no glyph for, except those in optional, which are left out and listed in report["dropped"]."""
    from PIL import Image, ImageDraw, ImageFont
    font_path = font_path or default_font()
    if not pathlib.Path(font_path).exists():
        raise AtlasError(f"font not found: {font_path} (GNU Unifont 16.0.04 otf; pass --font)")
    font = ImageFont.truetype(font_path, 16)

    def render(c):
        w = 16 if font.getlength(c) >= 16 else 8
        im = Image.new("L", (w, 16), 0)
        ImageDraw.Draw(im).text((0, 0), c, font=font, fill=255)
        return w, im

    notdef = render("\U0010FFFD")[1].tobytes()
    order = sorted(chars, key=ord)
    cps, idx, wide, glyphs, missing, widths = [], [], [], [], [], []
    for c in order:
        w, im = render(c)
        data = im.tobytes()
        if c != " " and unicodedata.category(c) != "Zs" and (not any(data) or (w == 8) == (len(notdef) == 128) and data == notdef):
            missing.append(c)
        tw = tv_width(c)
        if tw is not None and tw >= 1 and tw != w // 8:
            widths.append(c)
        px = im.load()
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
    dropped = [c for c in missing if c in optional]
    missing = [c for c in missing if c not in optional]
    if dropped:  # rebuild without them so the atlas holds no blank cells
        js, rep = bake({c: s for c, s in chars.items() if c not in dropped}, font_path)
        rep["dropped"] = dropped
        return js, rep
    if missing:
        raise AtlasError("the font has no glyph for these characters (the page would draw \"?\"):\n" + "\n".join(
            f"  U+{ord(c):04X} {c!r} {unicodedata.name(c, '?')}  from {chars[c]}" for c in missing))
    bits = base64.b64encode(b"".join(glyphs)).decode()
    js = ("window.WWDOS_ATLAS={cps:%s,idx:%s,wide:%s,n:%d,bits:\"%s\"};\n"
          % ("[" + ",".join(map(str, cps)) + "]", "[" + ",".join(map(str, idx)) + "]",
             "[" + ",".join(map(str, wide)) + "]", len(glyphs), bits))
    return js, {"chars": len(order), "glyphs": glyphs, "width_mismatch": widths, "dropped": []}


def width_report(film_root: pathlib.Path = FILM_ROOT) -> str:
    """Characters of the canon kaomoji (app/film/boot, the film root's stages/primers, the menu bar) with their width
    by Python's east-asian-width, the font's advance and Turbo Vision's wcwidth; mismatches first."""
    from PIL import ImageFont
    font = ImageFont.truetype(default_font(), 16)
    chars = {}
    for f in sorted(glob.glob(str(ROOT / BOOT)) + glob.glob(str(film_root / "stages/primers/*.txt"))):
        add(chars, pathlib.Path(f).read_text(encoding="utf-8"), os.path.relpath(f, ROOT))
    app = (ROOT / "app/film/minimal_app.cpp").read_text()
    for s in re.findall(r'"([^"]*[^\x00-\x7f][^"]*)"', app):
        add(chars, s, "minimal_app.cpp menu bar")
    rows = []
    for c in sorted(chars, key=ord):
        if ord(c) < 128:
            continue
        pw, fw, tw = py_width(c), 2 if font.getlength(c) >= 16 else 1, tv_width(c)
        rows.append((pw == fw == tw, f"U+{ord(c):04X} {c}  python {pw}  font {fw}  tvision {tw}  "
                                     f"{unicodedata.name(c, '?')}  ({chars[c]})"))
    bad = [r for ok, r in rows if not ok]
    return (f"{len(rows)} non-ASCII characters, {len(bad)} with a width disagreement\n" + "\n".join(bad)
            + ("\n" if bad else "") + "agree: " + "".join(r.split()[1] for ok, r in rows if ok))


# ---- the desktop atlas (kernel/pages/atlas.js) ----
def desktop_chars(extra: str) -> list:
    chars = set(CP437) | set(extra)
    files = (glob.glob(str(ROOT / PRIMERS / "*.txt"))
             + glob.glob(str(ROOT / "app/**/*.cpp"), recursive=True) + glob.glob(str(ROOT / "app/**/*.h"), recursive=True))
    for f in files:
        chars |= set(pathlib.Path(f).read_text(encoding="utf-8", errors="replace"))
    return sorted((c for c in chars if drawable(c)), key=ord)


def desktop_atlas(font_path: str, extra: str) -> tuple:
    """Return (atlas js, glyph cells, blank characters) for the desktop page. A blank glyph is reported, not refused."""
    from PIL import Image, ImageDraw, ImageFont
    font = ImageFont.truetype(font_path, 16)
    chars = desktop_chars(extra)
    cps, idx, wide, glyphs, missing = [], [], [], [], []
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
    return js, glyphs, missing


def write_sheet(glyphs: list, path: str) -> None:
    from PIL import Image
    cols = 32
    rows_n = (len(glyphs) + cols - 1) // cols
    sheet = Image.new("L", (cols * 8, rows_n * 16), 0)
    for g, rows in enumerate(glyphs):
        for y, b in enumerate(rows):
            for x in range(8):
                if b & (0x80 >> x):
                    sheet.putpixel(((g % cols) * 8 + x, (g // cols) * 16 + y), 255)
    sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(path)
    print("sheet:", path)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--font", default=default_font())
    ap.add_argument("--out", default="")
    ap.add_argument("--extra", default="")
    ap.add_argument("--film", action="store_true", help="the film atlas (default out kernel/pages/atlas-film.js)")
    ap.add_argument("--film-root", default=str(FILM_ROOT), help="where stages/ and scenes/ are (default tfilm/)")
    ap.add_argument("--chars-json", default="")
    ap.add_argument("--png", default="")
    ap.add_argument("--widths", action="store_true")
    a = ap.parse_args()
    film_root = pathlib.Path(a.film_root)
    if a.widths:
        print(width_report(film_root))
        return 0
    if not a.film and not a.chars_json:
        if not pathlib.Path(a.font).is_file():
            print(f"make_atlas.py: font not found: {a.font} (GNU Unifont 16.0.04 otf; pass --font or set UNIFONT)",
                  file=sys.stderr)
            return 3
        out = a.out or str(KERNEL / "pages/atlas.js")
        js, glyphs, missing = desktop_atlas(a.font, a.extra)
        pathlib.Path(out).write_text(js)
        print(f"{len(js):,} bytes js, {len(glyphs)} glyph cells; blank glyphs: {''.join(missing) or 'none'}")
        if a.png:
            write_sheet(glyphs, a.png)
        return 0
    if a.film and not a.chars_json and not film_sources(film_root):
        print(f"make_atlas.py: no film sources under {film_root} (stages/, scenes/): the atlas holds the interface set"
              " and the primers only", file=sys.stderr)
    out = a.out or str(KERNEL / "pages/atlas-film.js")
    chars = json.loads(pathlib.Path(a.chars_json).read_text()) if a.chars_json else shared_chars(a.extra, film_root)
    # The shared atlas serves pages built without a scene, which ship only the interface set; a library primer
    # character the font lacks is left out with a note (a scene that ships that primer fails in the film tools).
    optional = set() if a.chars_json else set(chars) - set(interface_chars()) - set(a.extra)
    try:
        js, rep = bake(chars, a.font, optional)
    except AtlasError as e:
        print(f"make_atlas.py: {e}", file=sys.stderr)
        return 3
    pathlib.Path(out).write_text(js)
    glyphs = rep["glyphs"]
    print(f"{rep['chars']} chars, {len(glyphs)} glyph cells, {len(js):,} bytes js")
    if rep["dropped"]:
        print("left out, no glyph in the font: " + " ".join(f"U+{ord(c):04X} {c} ({chars[c]})" for c in rep["dropped"]))
    if rep["width_mismatch"]:
        print("width differs between font and Turbo Vision (shifts or cuts a cell): "
              + " ".join(f"U+{ord(c):04X} {c}" for c in rep["width_mismatch"]))
    if a.png:
        write_sheet(glyphs, a.png)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
