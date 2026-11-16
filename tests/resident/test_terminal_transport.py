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
    """The last 80 acts (acts lists oldest first from since=)."""
    nxt = desk.j("acts", limit=1)["next"]
    return desk.ipc("acts", since=max(0, nxt - 80), limit=80)


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


def grab(desk, wid):
    desk.ipc("focus_window", id=wid)
    time.sleep(0.3)
    desk.person_keys("M-w")
    time.sleep(0.3)
    desk.person_keys("g")
    time.sleep(0.5)


def test_keyboard(desk):
    print("\n[keyboard] grab, the release chord, terminal_release")
    desk.ipc("close_all")
    wid = open_counter(desk)
    s = state(desk, wid)
    R.check(s["keyboard"] == "focus", f"a focused terminal reads keyboard=focus ({s['keyboard']})")
    R.check("Input Grab" not in title(desk, wid), f"no '(Input Grab)' on a terminal that does not hold the keyboard ({title(desk, wid)!r})")
    ed = desk.editor(70, 1, 40, 10, "other")
    time.sleep(0.2)
    R.check(state(desk, wid)["keyboard"] == "none", "an unfocused terminal reads keyboard=none")
    grab(desk, wid)
    s = state(desk, wid)
    R.check(s["keyboard"] == "grab", f"Window > Grab Keyboard: keyboard=grab ({s['keyboard']})")
    R.check("(Input Grab)" in title(desk, wid), f"the title says it holds the keyboard ({title(desk, wid)!r})")
    R.check("Alt-End" in " ".join(desk.screen()[-3:]), "the status line names Alt-End while grabbed")
    d = desk.j("describe", id=wid)
    R.check("holds the keyboard" in d.get("state", ""), "describe says the window holds the keyboard")
    desk.person_keys("F6")
    time.sleep(0.3)
    R.check(state(desk, wid)["keyboard"] == "grab", "F6 does not leave a grabbed terminal")
    desk.person_keys("M-p")
    time.sleep(0.4)
    R.check(state(desk, wid)["state"] == "paused", "Alt-P pauses while the terminal holds the keyboard")
    desk.person_keys("M-p")
    time.sleep(0.4)
    R.check(state(desk, wid)["state"] == "running", "Alt-P plays while the terminal holds the keyboard")
    desk.person_keys("M-End")
    time.sleep(0.4)
    s = state(desk, wid)
    R.check(s["keyboard"] == "focus" and "Input Grab" not in title(desk, wid), f"Alt-End gives the keyboard back ({s['keyboard']})")
    grab(desk, wid)
    R.check(state(desk, wid)["keyboard"] == "grab", "grabbed again")
    r = desk.j("terminal_release", id=wid, actor="wob", source="resident")
    time.sleep(0.4)
    R.check(r.get("released") and state(desk, wid)["keyboard"] != "grab", f"terminal_release gives it back ({r})")
    r = desk.j("terminal_release", id=wid)
    R.check(r.get("ok") and not r.get("released"), "terminal_release on a window that does not hold it changes nothing")
    R.check("gave the keyboard back" in acts_text(desk), "the act log records the release")
    desk.ipc("close_all")


ANIM = ("read x; for k in 0 1 2 3 4 5; do printf '\\033[2J\\033[Hframe %s\\n' $k; i=0; "
        "while [ $i -lt $k ]; do printf '#'; i=$((i+1)); done; printf '\\n'; sleep 1; done; sleep 60")


def lines_of(text):
    rows = [r.rstrip() for r in text.split("\n")]
    while rows and not rows[-1]:
        rows.pop()
    return rows


def player_lines(desk, pid):
    return lines_of("\n".join(desk.j("window_text", id=pid)["lines"]))


def test_casts(desk):
    print("\n[casts] terminal_record, open_cast, seek, play, pause, keys, frame controls")
    desk.ipc("close_all")
    path = f"/tmp/wibwob_{desk.name}_test.cast"
    wid = desk.j("open_terminal", exec=ANIM, x=2, y=1, w=40, h=10, title="anim")["id"]
    time.sleep(0.6)
    r = desk.j("terminal_record", id=wid, action="start", file=path)
    t0 = time.time()
    R.check(r.get("ok") and "(Rec)" in title(desk, wid), f"terminal_record start ({r})")
    desk.ipc("terminal_write", window_id=wid, text="\n")
    samples = {}
    for k in range(6):
        at = k + 0.5
        time.sleep(max(0, t0 + at - time.time()))
        samples[at] = lines_of(desk.ipc("terminal_read", window_id=wid))
    time.sleep(max(0, t0 + 6.3 - time.time()))
    r = desk.j("terminal_record", id=wid, action="stop")
    R.check(r.get("ok") and r.get("events", 0) >= 6, f"terminal_record stop ({r})")
    with open(path) as f:
        rows = f.read().splitlines()
    head = json.loads(rows[0])
    evs = [json.loads(x) for x in rows[1:]]
    R.check(head.get("version") == 2 and head.get("width") == 38 and head.get("height") == 8,
            f"the header is asciinema v2 with the terminal's size ({rows[0][:80]})")
    R.check(all(len(e) == 3 and e[1] == "o" and isinstance(e[2], str) for e in evs) and
            all(evs[i][0] <= evs[i + 1][0] for i in range(len(evs) - 1)), f"{len(evs)} [time, \"o\", data] lines in time order")
    R.check(samples[2.5][:1] == ["frame 2"], f"the terminal showed frame 2 at 2.5 s ({samples[2.5][:2]})")

    pid = desk.j("open_cast", file=path, x=60, y=1, paused="1")["id"]
    st = desk.j("cast_state", id=pid)
    R.check(desk.windows()[pid]["type"] == "cast_player" and not st["playing"], f"open_cast paused=1 opens a cast_player ({st})")
    same = []
    for at in sorted(samples):
        desk.j("cast_control", id=pid, seek=at)
        same.append(player_lines(desk, pid) == samples[at])
    R.check(all(same), f"seeking to each chosen second shows the screen the terminal showed then ({same})")
    desk.j("cast_control", id=pid, seek=1.5)
    R.check(player_lines(desk, pid) == samples[1.5], "seeking back re-reads the cast from the start")
    study = desk.ipc("look_study", id=pid)
    R.check("frame 1" in study, "look_study reads the player's cells")

    desk.j("cast_control", id=pid, seek=0, action="play", speed=2)
    time.sleep(1.2)
    st = desk.j("cast_state", id=pid)
    R.check(st["playing"] and st["pos"] >= 1.6, f"play at x2 advances the position ({st['pos']})")
    desk.j("cast_control", id=pid, action="pause")
    p1 = desk.j("cast_state", id=pid)["pos"]
    time.sleep(0.5)
    R.check(desk.j("cast_state", id=pid)["pos"] == p1, "pause holds the position")

    # The person: Space plays and pauses, Home goes to the start, a click on the control row's [►].
    desk.ipc("focus_window", id=pid)
    time.sleep(0.2)
    desk.person_keys("Space")
    time.sleep(0.4)
    R.check(desk.j("cast_state", id=pid)["playing"], "Space plays")
    desk.person_keys("Space")
    time.sleep(0.2)
    R.check(not desk.j("cast_state", id=pid)["playing"], "Space pauses")
    desk.person_keys("Home")
    time.sleep(0.2)
    R.check(desk.j("cast_state", id=pid)["pos"] == 0.0, "Home goes to the start")
    w = desk.windows()[pid]
    row_y = w["y"] + w["h"] - 2 + 1          # the control row, as a screen row
    row = desk.screen()[row_y]
    bx = row.find("[►]", w["x"])
    R.check(bx > 0, f"the control row shows [►] while paused ({row[w['x']:w['x'] + w['w']]!r})")
    desk.person_mouse("down", bx + 1, row_y)
    desk.person_mouse("up", bx + 1, row_y)
    time.sleep(0.4)
    R.check(desk.j("cast_state", id=pid)["playing"], "a click on [►] plays")
    desk.j("cast_control", id=pid, action="pause", loop=1, seek=5.9)
    desk.j("cast_control", id=pid, action="play", speed=1)
    time.sleep(1.0)
    st = desk.j("cast_state", id=pid)
    R.check(st["loop"] and st["playing"] and st["pos"] < 2, f"loop=1 starts again after the end ({st['pos']})")
    d = desk.j("describe", id=pid)
    R.check("cast player" in d.get("state", ""), f"describe ({d.get('state', '')[:70]})")

    # The worked example in the repo.
    ex = os.path.join(os.path.dirname(__file__), "..", "..", "casts", "bounce.cast")
    r = desk.j("open_cast", file=os.path.abspath(ex), paused="1")
    st = desk.j("cast_state", id=r["id"])
    desk.j("cast_control", id=r["id"], seek=3)
    R.check(st["events"] > 10 and st["duration"] > 5 and any("frame" in x for x in player_lines(desk, r["id"])),
            f"casts/bounce.cast plays ({st['events']} events, {st['duration']} s)")
    r = desk.ipc("open_cast", file="/etc/hosts")
    R.check(r.startswith("err"), f"a file that is not a cast is refused ({r[:60]})")
    desk.ipc("close_all")


GROUPS = {"commands": test_commands, "casts": test_casts, "keyboard": test_keyboard, "shell": test_shell, "person": test_person, "focus": test_focus, "close": test_close}


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
