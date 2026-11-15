#!/usr/bin/env python3
"""Resident input tests: keys, pointer, addressed input, manners, dialogs and menus by name.

Starts its own desktop (tmux -L <name>, socket /tmp/wibwob_<name>.sock) and stops it after.
Binary: WWDOS_BIN or build-resident/app/wwdos.

Usage:
    python3 tests/resident/test_resident_input.py            # all slices
    python3 tests/resident/test_resident_input.py s1 duet    # chosen groups
Groups: s1 (queue, burst, keys, pointer), s2 (focus, manners), duet, s3 (dialogs, menus).
"""

import json
import os
import random
import string
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import Desk, Results  # noqa: E402

R = Results()


def q(desk, cmd, **kv):
    """Queue a resident command and wait for every item it queued; return (reply, log entries)."""
    r = desk.ipc(cmd, **kv)
    if not r.startswith("{"):
        return r, {}
    j = json.loads(r)
    if not j.get("ok"):
        return r, {}
    return j, desk.wait_seqs(j["first"], j["last"])


def fresh(desk):
    desk.ipc("close_all")
    desk.ipc("resident_manners", clear="1", share_ms="1500")
    time.sleep(0.2)


# ---------------------------------------------------------------------------- S1

def test_s1(desk):
    print("\n[S1] queue, burst, keys, pointer")
    fresh(desk)
    a = desk.editor(2, 1, 70, 14, "A")
    b = desk.editor(80, 1, 70, 14, "B")
    R.check(desk.focused() == b, f"B ({b}) is focused after creation")

    # 200 keys in one call, unaddressed: they go where the person's keys go (B, focused).
    rnd = random.Random(7)
    burst = "".join(rnd.choice(string.ascii_letters + string.digits + "-_.,;!?") for _ in range(200))
    j, log = q(desk, "resident_key", text=burst)
    R.check(isinstance(j, dict) and j["count"] == 200, "200 keys queued in one call")
    order = [log[s]["what"][4:] for s in range(j["first"], j["last"] + 1)]
    R.check("".join(order) == burst, "the delivery log lists the 200 keys in order")
    R.check(desk.text(b) == [burst], "B holds the 200 characters, in order, none lost")

    # 200 keys in 200 separate calls, addressed to A (not focused).
    seq_first = None
    burst2 = "".join(rnd.choice(string.ascii_lowercase) for _ in range(200))
    t0 = time.time()
    for ch in burst2:
        r = json.loads(desk.ipc("resident_key", id=a, text=ch))
        seq_first = seq_first or r["first"]
        last = r["last"]
    desk.wait_seqs(seq_first, last)
    dt = time.time() - t0
    R.check(desk.text(a) == [burst2], f"A holds 200 keys sent in 200 calls, in order ({dt:.2f}s)")
    R.check(desk.focused() == b, "the person's focus stayed on B")

    # Named keys and modifiers on A.
    fresh(desk)
    a = desk.editor(2, 1, 70, 14, "A")
    b = desk.editor(80, 1, 70, 14, "B")
    q(desk, "resident_key", id=a, text="hello")
    q(desk, "resident_key", id=a, keys="Home")
    q(desk, "resident_key", id=a, text="X")
    q(desk, "resident_key", id=a, keys="End BS BS Enter")
    q(desk, "resident_key", id=a, text="two")
    q(desk, "resident_key", id=a, keys="Up Right Del")
    # "hello" Home "X" End BS BS Enter "two" -> ["Xhel", "two"]; Up Right Del joins the lines at the end of line 0.
    R.check(desk.text(a) == ["Xheltwo"], f"Home, End, Backspace, Enter, Up, Right, Del act in A: {desk.text(a)}")
    q(desk, "resident_key", id=a, keys="Enter")
    R.check(desk.text(a) == ["Xhel", "two"], f"Enter splits the line again at the cursor: {desk.text(a)}")
    bad = desk.ipc("resident_key", id=a, keys="Hyper+Q")
    R.check(bad.startswith("err unknown key"), f"an unknown key is refused: {bad}")
    for spec in ["Ctrl+A", "Alt+X", "Shift+Tab", "F1", "Ctrl+F12", "Alt+F3", "Ctrl+Left", "Shift+Del", "Space", "0x1c0d", "é"]:
        r = desk.ipc("resident_key", id=b, keys=spec)
        R.check(r.startswith("{"), f"key spec {spec!r} parses")
    time.sleep(0.3)

    # Pointer: drag A by its title bar, addressed; A moves, B keeps focus and stays on top.
    fresh(desk)
    a = desk.editor(2, 1, 50, 12, "A")
    b = desk.editor(30, 6, 50, 12, "B")   # B overlaps A and is focused
    w = desk.windows()
    ax, ay = w[a]["x"], w[a]["y"]
    j, log = q(desk, "resident_mouse", id=a, action="drag", x=ax + 10, y=ay, x2=ax + 30, y2=ay + 20)
    w = desk.windows()
    R.check((w[a]["x"], w[a]["y"]) == (ax + 20, ay + 20), f"addressed drag moved A by (20,20): {w[a]['x']},{w[a]['y']}")
    R.check(desk.focused() == b, "B kept the focus during the resident's drag")
    R.check(w[b]["z"] < w[a]["z"], "A was not raised above B")

    # Scroll: wheel on A's scroll bar moves its thumb (seen on screen).
    fresh(desk)
    a = desk.editor(2, 1, 40, 12, "A")
    b = desk.editor(60, 1, 40, 12, "B")
    q(desk, "resident_key", id=a, text="\n".join(f"line {i}" for i in range(60)))
    w = desk.windows()
    col = w[a]["x"] + w[a]["w"] - 2   # the scroll bar sits inside the right frame edge
    top = w[a]["y"] + 1
    def thumb():
        rows = desk.screen()
        for r in range(top + 1, top + w[a]["h"] - 1):
            line = rows[r] if r < len(rows) else ""
            if col < len(line) and line[col] == "■":
                return r
        return None
    t_before = thumb()
    j, log = q(desk, "resident_mouse", id=a, action="scroll", dir="up", count=40, x=col, y=w[a]["y"] + 5)
    time.sleep(0.2)
    t_after = thumb()
    R.check(t_before is not None and t_after is not None and t_after < t_before,
            f"40 wheel steps up moved A's scroll thumb up (row {t_before} -> {t_after})")
    R.check(desk.focused() == b, "B kept the focus during the scroll")


# ---------------------------------------------------------------------------- S2

def test_s2(desk):
    print("\n[S2] addressed input and manners")
    fresh(desk)
    a = desk.editor(2, 1, 70, 14, "A")
    b = desk.editor(80, 1, 70, 14, "B")
    c = desk.editor(2, 20, 70, 14, "C")
    desk.ipc("focus_window", id=a)
    R.check(desk.focused() == a, "the person's window is A")

    # The person's mouse button is down: resident input waits until it is released.
    w = desk.windows()
    px, py = w[a]["x"] + 5, w[a]["y"] + 1 + 5      # screen row = desktop row + 1 (menu bar)
    desk.person_mouse("down", px, py)
    time.sleep(0.3)
    r = json.loads(desk.ipc("resident_key", id=c, text="wait"))
    time.sleep(0.5)
    st = desk.j("resident_status")
    R.check(st["waiting"] == "person mouse button down" and st["queued"] == 4,
            f"while the person's button is down, 4 resident keys wait (queued={st['queued']}, waiting={st['waiting']!r})")
    R.check(desk.text(c) == [""], "C is unchanged while they wait")
    desk.person_mouse("up", px, py)
    desk.wait_seqs(r["first"], r["last"])
    R.check(desk.text(c) == ["wait"], "after the button is released the keys arrive in C")

    # Sharing violation: the person has just typed in A.
    desk.person_type("person")
    time.sleep(0.2)
    r = desk.ipc("resident_key", id=a, text="x")
    R.check(r.startswith("err Sharing violation"), f"resident keys to the window the person is typing in: {r}")
    r2 = desk.ipc("resident_key", id=b, text="ok")
    R.check(r2.startswith("{"), "resident keys to another window at the same moment are accepted")
    r3 = desk.ipc("resident_mouse", id=a, action="click", x=w[a]["x"] + 3, y=w[a]["y"] + 3)
    R.check(r3.startswith("err Sharing violation"), "a resident click on the person's window is refused too")
    time.sleep(1.7)
    r4 = desk.ipc("resident_key", id=a, text="!")
    R.check(r4.startswith("{"), "after share_ms of person idleness the window can be shared again")
    time.sleep(0.3)
    R.check(desk.text(a) == ["person!"], f"A holds the person's text then the resident's: {desk.text(a)}")
    R.check(desk.focused() == a, "focus never left A")

    # Unaddressed resident keys land in the person's window, so they are refused while the person types.
    desk.person_type("z")
    time.sleep(0.1)
    r5 = desk.ipc("resident_key", text="q")
    R.check(r5.startswith("err Sharing violation"), "unaddressed keys while the person types are refused")


# ---------------------------------------------------------------------------- duet

def test_duet(desk, rounds=6):
    print("\n[duet] a person and the resident type into two windows at the same time")
    fresh(desk)
    a = desk.editor(2, 1, 74, 16, "person")
    b = desk.editor(80, 1, 74, 16, "resident")
    desk.ipc("focus_window", id=a)
    person_chunks = [f"p{i:02d}-person " for i in range(rounds * 4)]
    errors = []
    seqs = []

    def resident():
        try:
            for i in range(rounds * 4):
                r = desk.ipc("resident_key", id=b, text=f"r{i:02d}xx")
                j = json.loads(r) if r.startswith("{") else None
                if not j:
                    errors.append(r)
                    return
                seqs.append((j["first"], j["last"]))
                r = desk.ipc("resident_key", id=b, keys="BS BS Home Ctrl+Right End Space")
                if not r.startswith("{"):
                    errors.append(r)
                    return
                j = json.loads(r)
                seqs.append((j["first"], j["last"]))
                time.sleep(0.01)
        except Exception as e:  # noqa: BLE001
            errors.append(repr(e))

    th = threading.Thread(target=resident)
    th.start()
    for chunk in person_chunks:
        desk.person_type(chunk)
    th.join()
    if seqs:
        desk.wait_seqs(seqs[0][0], seqs[-1][1])
    time.sleep(0.5)
    want_a = "".join(person_chunks)
    want_b = "".join(f"r{i:02d} " for i in range(rounds * 4))
    got_a = "".join(desk.text(a))
    got_b = "".join(desk.text(b))
    R.check(not errors, f"the resident's commands were accepted ({errors[:1]})")
    R.check(got_a == want_a, f"the person's {len(want_a)} characters arrived intact in their window ({len(got_a)} got)")
    R.check(got_b == want_b, f"the resident's {len(want_b)} characters (typed with Backspace, Home, End) arrived intact ({got_b[:40]!r})")
    R.check(desk.focused() == a, "the person's focus never moved")


# ---------------------------------------------------------------------------- S3

def test_s3(desk):
    print("\n[S3] what is in front; dialogs and menus by name")
    fresh(desk)
    a = desk.editor(2, 1, 70, 14, "A")
    b = desk.editor(80, 1, 70, 14, "B")

    # Menu items chosen by name are queued; inject_command (putEvent, one slot) is not.
    before = len(desk.windows())
    lines = "\n".join(["cmd:exec_command name=inject_command cmd_id=100"] * 5)
    import base64
    desk.ipc("batch", lines="base64:" + base64.b64encode(lines.encode()).decode())
    time.sleep(0.5)
    injected = len(desk.windows()) - before
    before = len(desk.windows())
    focus_before = desk.focused()
    lines = "\n".join(["cmd:resident_menu choose=File>New%20Test%20Pattern"] * 5)
    desk.ipc("batch", lines="base64:" + base64.b64encode(lines.encode()).decode())
    time.sleep(0.8)
    chosen = len(desk.windows()) - before
    print(f"    5 inject_command in one batch -> {injected} window(s); 5 resident_menu -> {chosen}")
    R.check(injected < 5, f"inject_command (single putEvent slot) loses events in a batch: {injected}/5")
    R.check(chosen == 5, f"resident_menu queues all five: {chosen}/5")
    R.check(desk.focused() == focus_before, "windows the resident opened did not take the person's focus")
    st = desk.j("resident_status")
    R.check(len(st["owned"]) >= 5, f"the resident owns the windows it opened ({len(st['owned'])})")
    fresh(desk)
    a = desk.editor(2, 1, 70, 14, "A")
    b = desk.editor(80, 1, 70, 14, "B")
    desk.ipc("focus_window", id=a)

    menus = desk.j("resident_menu")
    names = [m["name"] for m in menus["menus"]]
    R.check(names[:6] == ["File", "Edit", "View", "Window", "Tools", "Help"], f"menu bar listed by name: {names}")

    # The resident opens a dialog: it is the resident's, and the person keeps typing.
    j, _ = q(desk, "resident_menu", choose="Help>About WIBWOBWORLD")
    time.sleep(0.3)
    f = desk.j("resident_front")
    R.check(f["modal"] and f["front"]["kind"] == "dialog", "resident_front reports a modal dialog")
    R.check(f["front"]["owner"] == "resident", "the dialog is marked as the resident's")
    btns = [c["label"] for c in f["front"]["controls"] if c["kind"] == "button"]
    R.check(btns == ["OK"], f"its controls are listed by label: {btns}")
    R.check(f["front"].get("accept", {}).get("press") == "OK", "accept is named: press OK")
    texts = [c["text"] for c in f["front"]["controls"] if c["kind"] == "text"]
    R.check(any("WIBWOBWORLD" in t for t in texts), "its text is reported")
    desk.person_type("still typing")
    time.sleep(0.4)
    R.check(desk.text(a) == ["still typing"], f"the person's keys reach A while the resident's dialog is open: {desk.text(a)}")
    j, log = q(desk, "resident_press", label="OK")
    time.sleep(0.3)
    f = desk.j("resident_front")
    R.check(not f["modal"], "pressing OK by label closed the resident's dialog")
    R.check(desk.focused() == a, "the person's focus is still A")

    # The person opens a dialog: resident keys still reach another window.
    time.sleep(1.6)
    desk.person_keys("M-h")
    time.sleep(0.3)
    f = desk.j("resident_front")
    R.check(f["menu_open"] and f["front"]["kind"] == "menu", "the person's open menu is reported as a menu")
    items = [i["name"] for i in f["front"].get("items", [])]
    R.check("About WIBWOBWORLD" in items, f"with its items: {items[:4]}")
    desk.person_keys("a")
    time.sleep(0.4)
    f = desk.j("resident_front")
    R.check(f["modal"] and f["front"]["owner"] == "person", "the person's About dialog is in front, owned by the person")
    j, log = q(desk, "resident_key", id=b, text="behind the dialog")
    R.check(desk.text(b) == ["behind the dialog"], "resident keys reach B while the person's dialog is modal")
    r = desk.ipc("resident_press", label="OK")
    R.check(r.startswith("err Sharing violation"), f"pressing the person's dialog while they act in it is refused: {r[:60]}")
    time.sleep(1.7)
    j, log = q(desk, "resident_press", label="OK")
    time.sleep(0.3)
    R.check(not desk.j("resident_front")["modal"], "after share_ms the resident can answer the person's dialog")

    # An input dialog: set the field by its label, read it back, cancel.
    time.sleep(0.2)
    q(desk, "resident_menu", choose="File>Save Workspace As")
    time.sleep(0.3)
    f = desk.j("resident_front")
    inputs = [c for c in f["front"].get("controls", []) if c["kind"] == "input"]
    R.check(inputs and inputs[0]["label"] == "Name:", f"the input is reported with its label: {inputs[:1]}")
    q(desk, "resident_press", label="Name", value="resident-test")
    f = desk.j("resident_front")
    inputs = [c for c in f["front"].get("controls", []) if c["kind"] == "input"]
    R.check(inputs and inputs[0]["value"] == "resident-test", "the field was set by its label")
    R.check(f["front"].get("cancel", {}).get("press") == "Cancel", "cancel is named: press Cancel")
    q(desk, "resident_press", label="Cancel")
    time.sleep(0.3)
    R.check(not desk.j("resident_front")["modal"], "Cancel by label closed it")
    r = desk.ipc("resident_menu", choose="File>No Such Thing")
    R.check(r.startswith("err no menu item"), "an unknown menu item is refused by name")


def main():
    groups = [g.lower() for g in sys.argv[1:]] or ["s1", "s2", "duet", "s3"]
    desk = Desk(os.environ.get("RESIDENT_TEST_NAME", "residenttest")).start()
    try:
        for g in groups:
            {"s1": test_s1, "s2": test_s2, "duet": test_duet, "s3": test_s3}[g](desk)
    except Exception as e:  # noqa: BLE001
        R.check(False, f"exception: {e!r}")
    finally:
        R.check(desk.alive(), "the desktop is still running")
        desk.stop()
    print(f"\n{R.passed} passed, {R.failed} failed")
    sys.exit(1 if R.failed else 0)


if __name__ == "__main__":
    main()
