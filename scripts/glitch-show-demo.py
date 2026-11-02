#!/usr/bin/env python3
"""Drive a WIBWOB-DOS instance through the GLITCH duet's five beats with capture on.

Beats: boot echo, two hands, the castle, gravity, the droste. Uses only the show
endpoints (README "Show endpoints") and the wibwob-2026 shelf. Stdlib only.

    python3 scripts/glitch-show-demo.py --api http://127.0.0.1:8097 --run /tmp/show-run
    uv run tools/frames_to_png.py /tmp/show-run --out <dir> --beats --contact --mp4 show.mp4

The instance needs the build with chrome_set / move_many / tiny windows (any build since
the show layer); with an older build the chrome lines fail soft and windows stay >= 16x6.
"""
from __future__ import annotations

import argparse
import json
import time
import urllib.error
import urllib.request

SHELF = "modules/wibwob-2026/primers/"
API = "http://127.0.0.1:8097"


def call(method: str, path: str, body=None, timeout: float = 120):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(API + path, data=data, method=method,
                                 headers={"content-type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        print(f"  ! {method} {path} -> {e.code} {e.read()[:200]!r}")
        return {}


def post(path, body=None):
    return call("POST", path, body if body is not None else {})


def ids():
    return {w["id"]: w for w in call("GET", "/state")["windows"]}


def open_piece(name, x, y, w, h, title=""):
    """Open a shelf piece; return its window id. Window = piece size + 2 each way."""
    before = set(ids())
    post("/menu/command", {"command": "open_primer", "args": {
        "path": SHELF + name + ".txt", "x": x, "y": y, "w": w, "h": h, "title": title}})
    new = [i for i in ids() if i not in before]
    return new[-1] if new else None


def beat(label):
    post("/events/mark", {"label": label})
    print(f"-- {label}")


def main():
    global API
    ap = argparse.ArgumentParser()
    ap.add_argument("--api", default=API)
    ap.add_argument("--run", default="")
    ap.add_argument("--fast", action="store_true", help="shorter pauses")
    a = ap.parse_args()
    API = a.api.rstrip("/")
    pause = (lambda s: time.sleep(s * 0.4)) if a.fast else time.sleep

    post("/windows/close_all")
    post("/chrome", {"reset": True})
    post("/menu/command", {"command": "desktop_texture", "args": {"char": " "}})
    run = post("/capture/start", {"dir": a.run, "name": "glitch-show"} if a.run else {"name": "glitch-show"})
    print("run", run.get("run_dir"))

    # 1 BOOT ECHO: the desktop boots like the talk did --------------------------
    beat("boot-echo")
    post("/chrome", {"menu": " ", "status": " "})
    before = set(ids())
    post("/menu/command", {"command": "open_text_editor", "args": {"title": "BIOS"}})
    ed = [i for i in ids() if i not in before][-1]
    post(f"/windows/{ed}/move", {"x": 30, "y": 8, "w": 92, "h": 14})
    post(f"/windows/{ed}/send_text", {"content": (
        "WIBWOB-DOS BIOS  (c) the castle\n"
        "memory test ........ 640K  ok\n"
        "seeking drive A: for two faces and a cat\n"
        "found: wib  found: wob  found: scramble (asleep)\n"
        "boot."), "cps": 26, "actor": "wob", "mode": "replace"})
    post("/chrome", {"status": " two faces and a cat are on the desktop", "cps": 40, "actor": "wib"})
    pause(1.0)

    # 2 TWO HANDS: two cursors fight over the first window ----------------------
    beat("two-hands")
    post("/cursors/on", {})
    post("/cursors/wob", {"x": 140, "y": 30, "ms": 10})
    post("/cursors/wib", {"x": 10, "y": 30, "ms": 10})
    post(f"/windows/{ed}/glide", {"x": 128, "y": 30, "w": 30, "h": 8, "ms": 700, "ease": "out", "actor": "wob"})
    pair = open_piece("face-wib-and-wob-small-pair", 70, 10, 17, 11)
    post(f"/windows/{pair}/type", {"content": "ours", "cps": 6, "target": "title", "actor": "wib"})
    for (x, actor, ease) in ((20, "wib", "back"), (120, "wob", "out"), (30, "wib", "bounce"),
                             (110, "wob", "back"), (70, "wib", "inout")):
        post(f"/windows/{pair}/glide", {"x": x, "y": 10, "ms": 650, "ease": ease, "actor": actor})
    pause(0.6)

    # 3 THE CASTLE: windows become rooms, beings walk on the frames -------------
    beat("the-castle")
    post("/chrome", {"status": " salon  library  kitchen  chapel", "cps": 30, "actor": "wob"})
    post(f"/windows/{pair}/glide", {"x": 4, "y": 2, "ms": 600, "actor": "wob"})
    rooms = []
    cast = [("face-wib-and-wob-heads-only", "salon"), ("being-wobbler", "library"),
            ("being-niff-in-moss-coat", "kitchen"), ("scramble-sitting", "chapel")]
    top = open_piece("place-castle-battlements-strip", 50, 4, 40, 4)
    for k, (piece, room) in enumerate(cast):
        wid = open_piece(piece, 52, 8 + k * 7, 36, 7)
        rooms.append(wid)
        post(f"/windows/{wid}/type", {"content": room, "cps": 14, "target": "title",
                                      "actor": "wib" if k % 2 == 0 else "wob"})
    tower = open_piece("place-castle-tower-www-flag", 90, 19, 15, 16)
    niff = None
    before = set(ids())
    post("/menu/command", {"command": "open_primer", "args": {
        "path": SHELF + "being-niff-small.txt", "x": 88, "y": 34, "w": 10, "h": 2,
        "frameless": 1, "shadowless": 1}})
    new = [i for i in ids() if i not in before]
    niff = new[-1] if new else None
    if niff:  # Niff climbs the castle along the room frames, floor by floor
        for y in (27, 20, 13, 6):
            post(f"/windows/{niff}/glide", {"x": 87, "y": y, "ms": 500, "ease": "out", "actor": "scramble"})
            post(f"/windows/{niff}/glide", {"x": 54 + (y % 3) * 8, "y": y + 1, "ms": 400, "ease": "inout"})
            post(f"/windows/{niff}/glide", {"x": 87, "y": y, "ms": 400, "ease": "inout"})
    pause(1.0)

    # 4 GRAVITY: Wob closes a window properly; Wib cuts the floor ----------------
    beat("gravity")
    post("/cursors/wob", {"x": 52, "y": 4, "ms": 400})
    post(f"/windows/{top}/close")
    post("/chrome", {"status": " wob closed it properly", "cps": 40, "actor": "wob"})
    post("/cursors/wib", {"x": 80, "y": 42, "ms": 400})
    post("/chrome", {"status": " wib cut the floor", "cps": 40, "actor": "wib"})
    post("/windows/fall", {"actor": "wib", "g": 60, "bounce": 0.35, "jitter": 9, "max_s": 7})
    pause(1.2)

    # 5 THE DROSTE: the desktop inside itself, down to a sleeping cat -------------
    beat("droste")
    post("/chrome", {"menu": "  Feel  Edit  Wonder", "cps": 18, "actor": "wib"})
    post("/chrome", {"status": " ", "kaomoji": "つ◡‿◡‿◡༽つ"})
    post("/cursors/on", {"on": False})
    post("/windows/droste", {"depth": 6, "pause_ms": 700, "actor": "wob"})
    pause(2.0)
    beat("end")
    pause(0.5)
    res = post("/capture/stop")
    print("frames", res.get("frames"), "run", res.get("run_dir"))


if __name__ == "__main__":
    main()
