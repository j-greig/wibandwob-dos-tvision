# /// script
# requires-python = ">=3.10"
# dependencies = ["playwright==1.49.1"]
# ///
"""Sound in the browser build: the desktop page plays the machine's mixer through WebAudio.

usage: uv run tests/web/sound.py [dist-web/wwdos.html]
Uses the installed Google Chrome (channel "chrome"), headless. Builds nothing: run kernel/build-web.sh first.

Checks: the page installs its audio (Module.wwAudio) with no audio context until a gesture; the
first key starts the context; blocks from the mixer arrive while the desktop runs; a TR-808 opened
from the View menu and started with Space puts drum hits into the audio (a non-zero peak). The
browser's own autoplay rules decide when sound may start; the page does not override them.
"""
import pathlib, sys, time
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCREEN = """() => {
  const t = window.wwdosTerm; if (!t) return '';
  const b = t.buffer.active, out = [];
  for (let y = 0; y < t.rows; y++) { const l = b.getLine(y); out.push(l ? l.translateToString(true) : ''); }
  return out.join('\\n');
}"""
AUDIO = """() => { const M = window.wwdosModule; const A = M && M.wwAudio; if (!A) return null;
  return {ctx: !!A.ctx, state: A.ctx ? A.ctx.state : 'none', rate: A.ctx ? A.ctx.sampleRate : 0, pushed: A.pushed, peak: A.peak,
          now: performance.now()}; }"""

passed = failed = 0


def check(cond, msg):
    global passed, failed
    print(("  PASS: " if cond else "  FAIL: ") + msg)
    if cond:
        passed += 1
    else:
        failed += 1


def wait(page, fn, timeout=15.0):
    end = time.time() + timeout
    v = None
    while time.time() < end:
        v = fn()
        if v:
            return v
        time.sleep(0.1)
    return v


def main():
    page_path = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "dist-web" / "wwdos.html").resolve()
    with sync_playwright() as p:
        browser = p.chromium.launch(channel="chrome", headless=True)
        page = browser.new_page(viewport={"width": 1400, "height": 900})
        page.route("**/*", lambda r: r.continue_() if r.request.url.startswith("file:") or r.request.url.startswith("data:") else r.abort())
        page.goto(page_path.as_uri())
        check(bool(wait(page, lambda: "File" in page.evaluate(SCREEN))), "the desktop draws its menu bar")
        a = wait(page, lambda: page.evaluate(AUDIO), 5)
        check(a is not None and not a["ctx"], f"the page installs its audio and makes no audio context before a gesture ({a})")
        page.keyboard.press("Escape")
        a = wait(page, lambda: (lambda v: v if v and v["state"] == "running" else None)(page.evaluate(AUDIO)), 5)
        check(a is not None, f"the first key starts the audio context ({page.evaluate(AUDIO)})")
        a0 = page.evaluate(AUDIO)
        time.sleep(4.0)
        a = page.evaluate(AUDIO)
        rate = (a["pushed"] - a0["pushed"]) * 512 / ((a["now"] - a0["now"]) / 1000)
        # a 2048-sample render-ahead window lands on either side of the measurement, so a few blocks either way
        check(abs(rate - 22050) < 22050 * 0.15, f"the mixer's blocks arrive at real time while the desktop runs ({rate:.0f} samples a second)")
        peak0 = a["peak"]
        page.keyboard.press("Alt+v")
        time.sleep(0.4)
        page.keyboard.press("8")
        ok = wait(page, lambda: "R H Y T H M" in page.evaluate(SCREEN), 5)
        check(bool(ok), "View > TR-808 opens the drum machine")
        page.keyboard.press("Space")
        time.sleep(1.5)
        check("PLAY" in page.evaluate(SCREEN), "Space starts the drum machine")
        a = page.evaluate(AUDIO)
        check(peak0 == 0 and a["peak"] > 0.05, f"Space starts it and its hits reach WebAudio (peak {peak0} then {a['peak']:.3f})")
        browser.close()
    print(f"\n{passed} passed, {failed} failed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
