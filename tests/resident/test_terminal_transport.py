#!/usr/bin/env python3
"""Terminal transport tests: pause and resume what runs in a terminal window, by command and by the person.

Starts its own desktop (tmux -L <name>, socket /tmp/wibwob_<name>.sock), no model. Binary: WWDOS_BIN or
build-resident/app/wwdos. Name: TERM_TEST_NAME (default termtest).

Usage:
    python3 tests/resident/test_terminal_transport.py              # all groups
    python3 tests/resident/test_terminal_transport.py commands person
Groups: commands (pause, resume, toggle, state, Feed), person (Alt-P, the frame mark, the Window menu),
focus (a paused program stays paused while focus moves), close (closing a paused window ends it),
keyboard (grab, release chord, terminal_release), casts (record, play, seek).
"""

import json
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import Desk, Results  # noqa: E402

R = Results()
MARK = f"ttc{os.getpid()}"
COUNTER = f"i=0; while :; do echo {MARK} n=$i; i=$((i+1)); sleep 0.1; done"


def q(desk, cmd, **kv):
    """A resident command through the queue; waits until every item it queued is delivered."""
    r = desk.ipc(cmd, **kv)
    try:
        j = json.loads(r)
    except json.JSONDecodeError:
        return r
    if j.get("ok") and "first" in j:
        desk.wait_seqs(j["first"], j["last"])
        time.sleep(0.1)
    return j


def last_n(desk, wid):
    out = desk.ipc("terminal_read", window_id=wid)
    nums = [int(m) for m in re.findall(MARK + r" n=(\d+)", out)]
    return (nums[-1] if nums else -1), nums


def state(desk, wid):
    return desk.j("terminal_state", id=wid)


def open_counter(desk, **kv):
    r = desk.j("open_terminal", exec=COUNTER, x=2, y=1, w=60, h=30, title="counter", **kv)
    wid = r["id"]
    t0 = time.time()
    while time.time() - t0 < 5 and last_n(desk, wid)[0] < 3:
        time.sleep(0.1)
    return wid


def frozen(desk, wid, wait=1.0):
    time.sleep(0.3)
    a, _ = last_n(desk, wid)
    time.sleep(wait)
    b, _ = last_n(desk, wid)
    return a, b


def counting_on(desk, wid, since, wait=0.6):
    time.sleep(wait)
    n, nums = last_n(desk, wid)
    return n > since and (since + 1) in nums, n


def acts_text(desk):
    return desk.ipc("acts", limit=60)


def title(desk, wid):
    return desk.windows()[wid]["title"]


def test_commands(desk):
    print("\n[commands] terminal_pause, terminal_resume, terminal_toggle, terminal_state")
    desk.ipc("close_all")
    wid = open_counter(desk)
    s = state(desk, wid)
    R.check(s["state"] == "running", f"state running at start ({s})")
    R.check(s["program"] in ("sh", "bash", "zsh"), f"program named ({s['program']})")
    r = desk.j("terminal_pause", id=wid, actor="wob", source="resident")
    R.check(r.get("ok") and r.get("changed") and r.get("state") == "paused", f"terminal_pause pauses ({r})")
    a, b = frozen(desk, wid)
    R.check(a == b and a >= 0, f"the counter stops while paused ({a} -> {b})")
    s = state(desk, wid)
    R.check(s["state"] == "paused" and s["seconds_paused"] >= 1.0, f"state paused with seconds ({s})")
    R.check("(Paused)" in title(desk, wid), f"title says paused ({title(desk, wid)!r})")
    d = desk.j("describe", id=wid)
    R.check("paused" in d.get("state", ""), f"describe says paused ({d.get('state', '')[-90:]!r})")
    r = desk.j("terminal_pause", id=wid)
    R.check(r.get("ok") and not r.get("changed"), "a second pause changes nothing")
    r = desk.j("terminal_resume", id=wid, actor="wib", source="resident")
    R.check(r.get("ok") and r.get("changed") and r.get("state") == "running", f"terminal_resume resumes ({r})")
    ok, n = counting_on(desk, wid, b)
    R.check(ok, f"the counter continues from where it stopped ({b} -> {b + 1} ... {n})")
    R.check("(Paused)" not in title(desk, wid), "title no longer says paused")
    r = desk.j("terminal_toggle")
    R.check(r.get("state") == "paused", "terminal_toggle with no id pauses the only terminal")
    r = desk.j("terminal_toggle")
    R.check(r.get("state") == "running", "terminal_toggle again resumes it")
    acts = acts_text(desk)
    R.check("paused the terminal" in acts and '"wob"' in acts, "the act log records wob pausing")
    R.check("resumed the terminal" in acts and '"wib"' in acts, "the act log records wib resuming")
    r = desk.ipc("terminal_pause", id="w999")
    R.check(r.startswith("err"), f"an unknown id is refused ({r})")
    desk.ipc("close_all")


def test_person(desk):
    print("\n[person] Alt-P, the frame mark, the Window menu")
    desk.ipc("close_all")
    wid = open_counter(desk)
    desk.ipc("focus_window", id=wid)
    time.sleep(0.2)
    desk.person_keys("M-p")
    time.sleep(0.4)
    R.check(state(desk, wid)["state"] == "paused", "Alt-P from the person pauses the focused terminal")
    a, b = frozen(desk, wid, 0.8)
    R.check(a == b, f"the counter stops ({a} -> {b})")
    R.check("Alt-P" in " ".join(desk.screen()[-3:]), "the status line names Alt-P")
    desk.person_keys("M-p")
    time.sleep(0.2)
    ok, n = counting_on(desk, wid, b)
    R.check(state(desk, wid)["state"] == "running" and ok, f"Alt-P again resumes from where it was ({b} -> {n})")
    acts = acts_text(desk)
    R.check('"person"' in acts and "paused the terminal" in acts, "the act log records the person pausing")

    # The mark on the frame: [‖] beside the zoom icon, desktop row y is screen row y + 1.
    w = desk.windows()[wid]
    mx, my = w["x"] + w["w"] - 9, w["y"] + 1
    row = desk.screen()[my]
    R.check("[‖]" in row, "the frame shows [‖] while running")
    desk.person_mouse("down", mx + 1, my)
    desk.person_mouse("up", mx + 1, my)
    time.sleep(0.4)
    R.check(state(desk, wid)["state"] == "paused", "a click on the mark pauses")
    R.check("[►]" in desk.screen()[my], "the frame shows [►] while paused")
    desk.person_mouse("down", mx + 1, my)
    desk.person_mouse("up", mx + 1, my)
    time.sleep(0.4)
    R.check(state(desk, wid)["state"] == "running", "a second click plays")

    # A resident clicks the same mark, and presses Alt-P addressed to the window. Residents wait
    # share_ms (1.5 s) after the person's last act in a window (docs/resident-input.md).
    time.sleep(1.7)
    q(desk, "resident_mouse", id=wid, action="click", x=mx, y=w["y"], actor="scramble", source="resident")
    R.check(state(desk, wid)["state"] == "paused", "a resident's click on the mark pauses")
    q(desk, "resident_key", id=wid, keys="Alt+P", actor="scramble", source="resident")
    R.check(state(desk, wid)["state"] == "running", "a resident's Alt+P to the window plays")

    # The Window menu entry, chosen by name.
    r = q(desk, "resident_menu", choose="Window>Pause or Play Terminal", id=wid)
    R.check(state(desk, wid)["state"] == "paused", f"Window > Pause or Play Terminal pauses ({str(r)[:60]})")
    acts = acts_text(desk)
    R.check('"scramble"' in acts, "the act log records scramble's pause and play")
    desk.ipc("terminal_resume", id=wid)
    desk.ipc("close_all")


def test_focus(desk):
    print("\n[focus] paused survives the window losing and regaining focus")
    desk.ipc("close_all")
    wid = open_counter(desk)
    desk.ipc("terminal_pause", id=wid)
    ed = desk.editor(70, 1, 40, 10, "other")
    desk.ipc("focus_window", id=ed)
    time.sleep(0.3)
    R.check(desk.focused() == ed, "focus moved to another window")
    a, b = frozen(desk, wid, 0.8)
    R.check(state(desk, wid)["state"] == "paused" and a == b, f"still paused while unfocused ({a} -> {b})")
    desk.person_keys("F6")
    time.sleep(0.3)
    desk.ipc("focus_window", id=wid)
    time.sleep(0.3)
    R.check(desk.focused() == wid, "focus back on the terminal")
    a2, b2 = frozen(desk, wid, 0.8)
    R.check(state(desk, wid)["state"] == "paused" and a2 == b2 == b, f"still paused after focus returns ({b2})")
    desk.ipc("terminal_resume", id=wid)
    ok, n = counting_on(desk, wid, b2)
    R.check(ok, f"resumes from where it was ({b2} -> {n})")
    desk.ipc("close_all")


def test_close(desk):
    print("\n[close] closing a paused window ends its program")
    desk.ipc("close_all")
    wid = open_counter(desk)
    desk.ipc("terminal_pause", id=wid)
    time.sleep(0.3)
    alive = subprocess.run(["pgrep", "-f", MARK], capture_output=True, text=True).stdout.split()
    R.check(len(alive) >= 1, f"the paused program exists ({alive})")
    desk.ipc("close_window", id=wid)
    gone = False
    for _ in range(40):
        time.sleep(0.1)
        if not subprocess.run(["pgrep", "-f", MARK], capture_output=True, text=True).stdout.split():
            gone = True
            break
    R.check(gone, "no process of the paused program is left after close")


def test_shell(desk):
    print("\n[shell] a job typed into an interactive shell (a pipeline) pauses and resumes")
    desk.ipc("close_all")
    r = desk.j("open_terminal", x=2, y=1, w=70, h=30, title="shell")
    wid = r["id"]
    time.sleep(1.5)
    desk.ipc("terminal_write", window_id=wid,
             text=f"( i=0; while :; do echo {MARK} n=$i; i=$((i+1)); sleep 0.1; done ) | cat\n")
    t0 = time.time()
    while time.time() - t0 < 6 and last_n(desk, wid)[0] < 3:
        time.sleep(0.1)
    R.check(last_n(desk, wid)[0] >= 3, "the job runs in the shell")
    s = desk.j("terminal_pause", id=wid)
    R.check(s.get("state") == "paused", f"paused ({s})")
    a, b = frozen(desk, wid)
    R.check(a == b and a >= 0, f"every process of the job stops ({a} -> {b})")
    desk.j("terminal_resume", id=wid)
    ok, n = counting_on(desk, wid, b)
    R.check(ok, f"the job continues in the foreground ({b} -> {n})")
    out = desk.ipc("terminal_read", window_id=wid)
    R.check("suspended" not in out and "stopped" not in out.lower(), "the shell did not take the job off the terminal")
    desk.ipc("terminal_write", window_id=wid, text="\x03")
    desk.ipc("close_all")


GROUPS = {"commands": test_commands, "shell": test_shell, "person": test_person, "focus": test_focus, "close": test_close}


def main():
    want = sys.argv[1:] or list(GROUPS)
    desk = Desk(os.environ.get("TERM_TEST_NAME", "termtest"))
    desk.start()
    try:
        for g in want:
            GROUPS[g](desk)
    finally:
        desk.stop()
    print(f"\n{R.passed} passed, {R.failed} failed")
    sys.exit(1 if R.failed else 0)


if __name__ == "__main__":
    main()
