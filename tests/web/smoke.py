# /// script
# requires-python = ">=3.10"
# dependencies = ["playwright==1.49.1"]
# ///
"""Headless smoke test of the browser pages built by kernel/build-web.sh.

usage: uv run tests/web/smoke.py [PAGE.html ...]     (default: every dist-web/*.html)
Uses the installed Google Chrome (channel "chrome"); set SMOKE_CHANNEL= (empty) to use Playwright's
own Chromium after `uv run --with playwright==1.49.1 playwright install chromium`.

For each page: load it from file:// with every other request blocked and counted, wait for the
menu bar ("File"; "Hello" in the demo), open the first menu by its hot key and check an item draws,
then open a window: the Greeting dialog in the demo; in the desktop pages (wwdos*.html) check the
start-screen windows and open a Test Pattern window with Ctrl+N. Exit 0 when every page passes.
"""
import os, pathlib, sys, time
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCREEN = """() => {
  const t = window.wwdosTerm; if (!t) return '';
  const b = t.buffer.active, out = [];
  for (let y = 0; y < t.rows; y++) { const l = b.getLine(y); out.push(l ? l.translateToString(true) : ''); }
  return out.join('\\n');
}"""


def wait_for(page, pred, timeout=15.0):
    end = time.time() + timeout
    text = ""
    while time.time() < end:
        text = page.evaluate(SCREEN)
        if pred(text):
            return True, text
        time.sleep(0.1)
    return False, text


def check(browser, path: pathlib.Path) -> list:
    errors, requests = [], []
    page = browser.new_page(viewport={"width": 1280, "height": 800})
    url = path.resolve().as_uri()
    def route(r):
        if r.request.url == url:
            r.continue_()
        else:
            requests.append(r.request.url); r.abort()
    page.route("**/*", route)
    page.on("pageerror", lambda e: errors.append(f"page error: {e}"))
    t0 = time.time()
    page.goto(url)
    first = "Hello" if path.name.startswith("hello") else "File"   # Turbo Vision's demo has one menu
    ok, text = wait_for(page, lambda s: first in s.split("\n")[0])
    if not ok:
        return [f"menu bar never showed {first!r}; first row: {text.splitlines()[:1]}"] + errors
    boot = time.time() - t0
    demo = path.name.startswith("hello")
    item = "Greeting" if demo else "New Test Pattern"
    page.keyboard.press("Alt+h" if demo else "Alt+f")   # open the first menu by its hot key
    ok, text = wait_for(page, lambda s: item in s, 5)
    if not ok:
        errors.append(f"the first menu did not open (no {item!r} on screen)")
    if demo:
        page.keyboard.press("Enter")  # Greeting... opens a dialog
        ok, text = wait_for(page, lambda s: "Hello, World" in s, 5)
        if not ok:
            errors.append("Greeting dialog did not open")
    else:
        page.keyboard.press("Escape")
        ok, before = wait_for(page, lambda s: "the island" in s and "the wibblers" in s and item not in s, 5)
        if not ok:
            errors.append("start-screen windows not on screen")
        # File > New Test Pattern (Ctrl+N) opens a window; the active window has the double frame
        # with the close box, so its top edge moves to the new window.
        top = lambda s: [i for i, l in enumerate(s.split("\n")) if "╔═[■]" in l]
        page.keyboard.press("Control+n")
        ok, text = wait_for(page, lambda s: top(s) and top(s) != top(before), 5)
        if not ok:
            errors.append("Ctrl+N did not open a new active window")
    if requests:
        errors.append(f"{len(requests)} network requests: {requests[:3]}")
    print(f"{path.name}: boot to menu {boot:.2f}s, renderer {page.evaluate('window.wwdosRenderer')}, "
          f"{'ok' if not errors else 'FAIL'}")
    page.close()
    return errors


def main() -> int:
    pages = [pathlib.Path(p) for p in sys.argv[1:]] or sorted((ROOT / "dist-web").glob("*.html"))
    if not pages:
        print("no pages: run kernel/build-web.sh first"); return 2
    failed = 0
    with sync_playwright() as p:
        channel = os.environ.get("SMOKE_CHANNEL", "chrome") or None
        browser = p.chromium.launch(channel=channel, headless=True)
        for path in pages:
            errs = check(browser, path)
            for e in errs:
                print("  " + e)
            failed += bool(errs)
        browser.close()
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
