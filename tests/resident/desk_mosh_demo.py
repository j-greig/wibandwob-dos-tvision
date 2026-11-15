#!/usr/bin/env python3
"""Desk mosh demonstration: a desktop with an editor, a primer and the TR-808 playing,
moshing through the five ways on the drum steps, recorded as text snapshots.

    python3 tests/resident/desk_mosh_demo.py [--out DIR] [--seconds 20] [--fps 5] [--keep]

Starts its own desktop (tmux -L moshdemo, EGA theme, 160x50, no sound), turns the mosh on
(way all: wrap, tear, mosh, bleed, raw, two bars each), types into the editor now and then
as a person would, and writes one tmux capture-pane -e snapshot per frame to DIR
(frame-0000.ans ...). At the end it turns the mosh off and checks the screen came back.
--keep leaves the desktop running: watch it with  tmux -L moshdemo attach.
Binary: WWDOS_BIN or build-resident/app/wwdos.
"""

import argparse
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import Desk, REPO  # noqa: E402

PRIMER = os.path.join(REPO, "modules", "wibwob-2026", "primers", "being-wibblers-im-happy.txt")


def cap(d):
    return subprocess.run(["tmux", "-L", d.name, "capture-pane", "-p", "-N", "-e", "-t", d.name],
                          capture_output=True, text=True).stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/desk-mosh-demo")
    ap.add_argument("--seconds", type=float, default=20)
    ap.add_argument("--fps", type=float, default=5)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--amount", default="85")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    d = Desk("moshdemo", cols=160, rows=50, env={"WWDOS_THEME": "ega"}).start()
    time.sleep(1.0)
    ed = d.editor(3, 2, 54, 16, title="notes")
    d.j("create_window", type="frame_player", path=PRIMER, x=62, y=2, w=44, h=16, title="the wibblers")
    d.j("create_window", type="tr808", x=21, y=19, w=118, h=28, preset="electro", play="1")
    d.ipc("focus_window", id=ed)
    time.sleep(0.6)
    d.person_type("wib & wob are typing while the desk moshes\r")
    time.sleep(0.4)
    before = cap(d)
    d.ipc("exec_command", name="mosh_amount", value=a.amount)
    d.ipc("exec_command", name="mosh_way", way="all")
    d.ipc("exec_command", name="mosh_on", actor="demo", source="resident")
    lines = ["the windows tear\r", "and drift\r", "and bleed\r", "still typing\r"]
    n = int(a.seconds * a.fps)
    t0 = time.time()
    for k in range(n):
        if k % int(a.fps * 4) == int(a.fps * 2):
            d.person_type(lines[(k // int(a.fps * 4)) % len(lines)], gap=0.05)
        open(os.path.join(a.out, f"frame-{k:04d}.ans"), "w").write(cap(d))
        time.sleep(max(0, t0 + (k + 1) / a.fps - time.time()))
    st = d.ipc("exec_command", name="mosh_status")
    print(st)
    d.ipc("exec_command", name="mosh_off", actor="demo", source="resident")
    time.sleep(0.6)
    print("editor text:", d.text(ed)[:6])
    print(f"{n} frames in {a.out}")
    if not a.keep:
        d.stop()
    else:
        print("running: tmux -L moshdemo attach   (Alt-M toggles the mosh, Esc Esc stops it)")


if __name__ == "__main__":
    main()
