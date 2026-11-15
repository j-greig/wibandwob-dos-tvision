#!/usr/bin/env python3
"""Inline xterm.js, its CSS and an Emscripten SINGLE_FILE .js into one html page.

usage: bundle.py <app.js> <out.html> [--shell kernel/pages/shell-webgl.html] [--atlas kernel/pages/atlas.js]

The default shell (pages/shell.html) inlines xterm.js. pages/shell-webgl.html has no xterm: its
<!--INLINE_ATLAS--> marker takes pages/atlas.js (tools/make_atlas.py) and the page draws cells itself with WebGL2.
--atlas names another atlas (the film page, pages/shell-film.html, takes pages/atlas-film.js).
"""
import gzip, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
WEB = ROOT / "pages"


def script(text: str) -> str:
    # A literal "</script" inside inlined JS would end the tag early.
    return "<script>\n" + text.replace("</script", "<\\/script") + "\n</script>"


def main() -> int:
    if "-h" in sys.argv[1:] or "--help" in sys.argv[1:]:
        print(__doc__)
        return 0
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    app_js, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    shell_path = pathlib.Path(sys.argv[sys.argv.index("--shell") + 1]) if "--shell" in sys.argv else WEB / "shell.html"
    shell = shell_path.read_text()
    atlas = pathlib.Path(sys.argv[sys.argv.index("--atlas") + 1]) if "--atlas" in sys.argv else WEB / "atlas.js"
    css = (WEB / "vendor/xterm.css").read_text()
    xterm = (WEB / "vendor/xterm.js").read_text() + "\n" + (WEB / "vendor/addon-fit.js").read_text() + "\n" + (WEB / "vendor/addon-webgl.js").read_text()
    page = shell.split("\n", 1)[1]  # drop the template comment line
    page = page.replace("<!--INLINE_CSS-->", "<style>\n" + css + "\n</style>")
    page = page.replace("<!--INLINE_XTERM-->", script(xterm))
    page = page.replace("<!--INLINE_ATLAS-->", script(atlas.read_text()))
    page = page.replace("<!--INLINE_APP-->", script(app_js.read_text()))
    html = "<!doctype html>\n<html lang=\"en\">\n" + page + "\n</html>\n"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(html)
    raw = out.stat().st_size
    gz = len(gzip.compress(html.encode(), 9))
    print(f"{out}: {raw:,} bytes raw, {gz:,} bytes gzip -9")
    return 0


if __name__ == "__main__":
    sys.exit(main())
