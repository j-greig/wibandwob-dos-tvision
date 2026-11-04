#!/usr/bin/env python3
"""tape_play.py: play a film tape (modules/wibwob-2026/tapes/<slug>.tape.gz) in the terminal it runs in.

    python3 tools/tapes/tape_play.py <file.tape.gz> [--at EPOCH] [--loop] [--stats FILE]

Hides the cursor, clears the screen, waits until EPOCH (wall clock, float seconds, time.time()) when given, then
writes frame k at EPOCH + k/fps. Every frame is scheduled from EPOCH, never from the frame before, so the tape
keeps time with audio started at the same EPOCH. A frame whose time has passed is dropped and the player jumps to
the frame for now. Only the rows that differ from the frame on screen are written (each row of a tape starts with
its own cursor position and colours), which keeps a busy terminal emulator inside its budget.
--loop repeats the tape (frame k % frames). At the end, or on SIGINT/SIGTERM/SIGHUP, it resets
colours, shows the cursor and exits 0. It prints nothing but the frames. --stats FILE writes a JSON summary
(frames shown, dropped, worst lateness) for timing tests. Standard library only.
"""
import gzip
import json
import os
import re
import signal
import sys
import time


def args(argv):
    path, at, loop, stats = None, None, False, None
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--at":
            at = float(argv[i + 1]); i += 2; continue
        if a.startswith("--at="):
            at = float(a.split("=", 1)[1])
        elif a == "--loop":
            loop = True
        elif a == "--stats":
            stats = argv[i + 1]; i += 2; continue
        elif a.startswith("--stats="):
            stats = a.split("=", 1)[1]
        elif path is None and not a.startswith("--"):
            path = a
        else:
            sys.stderr.write(__doc__); sys.exit(2)
        i += 1
    if path is None:
        sys.stderr.write(__doc__); sys.exit(2)
    return path, at, loop, stats


def load(path):
    with gzip.open(path, "rb") as z:
        head = json.loads(z.readline())
        frames = [ln.rstrip(b"\n") for ln in z]
    frames = [f for f in frames if f][: int(head["frames"])]
    return head, frames


ROW = re.compile(rb"\x1b\[\d+;1H")


def rows_of(frame):
    """a frame -> its rows, each starting with its own ESC[r;1H (the tape writes every row that way)."""
    starts = [m.start() for m in ROW.finditer(frame)]
    if not starts:
        return [frame]
    return [frame[a:b] for a, b in zip(starts, starts[1:] + [len(frame)])]


class Stop(Exception):
    pass


def main(argv):
    path, at, loop, stats_path = args(argv)
    head, frames = load(path)
    fps = float(head["fps"])
    frames = [rows_of(f) for f in frames]
    n = len(frames)
    fd = sys.stdout.fileno()

    def put(b):
        view = memoryview(b)
        while view:
            k = os.write(fd, view)
            view = view[k:]

    def stop(*_):
        raise Stop
    for s in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        signal.signal(s, stop)
    st = dict(frames=n, fps=fps, shown=0, dropped=0, late_max=0.0, expected=0, epoch=None, ended=None)
    try:
        put(b"\x1b[0m\x1b[?25l\x1b[2J\x1b[H")
        epoch = time.time() if at is None else at
        st["epoch"] = epoch
        k = 0
        shown = None                                       # the rows on screen now
        while True:
            if not loop and k >= n:
                break
            target = epoch + k / fps
            now = time.time()
            if now < target:
                time.sleep(target - now)
                now = time.time()
            due = int((now - epoch) * fps)                 # the frame whose time this is
            if due > k:                                    # late: drop to the frame for now
                st["dropped"] += due - k
                k = due
                if not loop and k >= n:
                    break
            st["late_max"] = max(st["late_max"], now - (epoch + k / fps))
            rows = frames[k % n]
            if shown is None or len(shown) != len(rows):
                put(b"".join(rows))
            else:
                put(b"".join(r for r, o in zip(rows, shown) if r != o))
            shown = rows
            st["shown"] += 1
            k += 1
        st["expected"] = n if not loop else k
    except Stop:
        pass
    except (BrokenPipeError, OSError):
        stats_path and write_stats(stats_path, st)
        return 0
    try:
        put(b"\x1b[0m\x1b[?25h")
    except OSError:
        pass
    st["ended"] = time.time()
    if stats_path:
        write_stats(stats_path, st)
    return 0


def write_stats(path, st):
    try:
        with open(path, "w") as f:
            json.dump(st, f)
    except OSError:
        pass


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
