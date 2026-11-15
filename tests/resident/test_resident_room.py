#!/usr/bin/env python3
"""Resident room tests: several minds in one room.

Starts its own desktops (tmux -L <ROOM_TEST_NAME>..., default roomtest) without a model: the claude CLI is
kept off PATH, and the Wib&Wob chat answers with a stand-in reply (WWDOS_CHAT_STANDIN_MS) so it is busy for
a known time. Binary: WWDOS_BIN or build-resident/app/wwdos.

Usage:
    python3 tests/resident/test_resident_room.py            # all groups
    python3 tests/resident/test_resident_room.py chat       # chosen groups
Groups: chat (typing while a reply streams), feed (waits, hands, mosh steps in the Feed), child (a child
process that died is a system line), teach (errors that teach, help, the resident's self-test).
"""

import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import Desk, Results  # noqa: E402

R = Results()
NAME = os.environ.get("ROOM_TEST_NAME", "roomtest")
NO_LLM = {"PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "ANTHROPIC_API_KEY": "", "WWDOS_CHAT_STANDIN_MS": "2500"}


def wait_for(pred, timeout=10.0, step=0.15):
    t0 = time.time()
    while time.time() - t0 < timeout:
        v = pred()
        if v:
            return v
        time.sleep(step)
    return None


def chat_lines(desk, cid):
    return desk.j("describe", id=cid)["text"]


def test_chat(desk):
    print("chat")
    cid = desk.j("create_window", type="wibwob", x=2, y=1, w=80, h=24, actor="setup")["id"]
    desk.ipc("focus_window", id=cid)
    time.sleep(0.5)

    def say(text):
        desk.person_type(text)
        desk.person_keys("Enter")
        time.sleep(0.15)

    say("one")
    time.sleep(0.3)
    say("two")
    say("three")
    d = desk.j("describe", id=cid)
    waiting = [l for l in d["text"] if l.startswith("waiting: ")]
    R.check(waiting == ["waiting: two", "waiting: three"] and "2 lines waiting" in d["state"],
            f"lines typed while the reply streams are kept and shown as waiting: {waiting} / {d['state']!r}")
    done = wait_for(lambda: (lambda t: t if sum("stand-in reply to:" in l for l in t) == 3 else None)(chat_lines(desk, cid)), 14)
    order = [l for l in (done or chat_lines(desk, cid)) if l.startswith("User: ") or "stand-in reply to:" in l]
    want = ["User: one", "reply one", "User: two", "reply two", "User: three", "reply three"]
    got = [("reply " + l.split("stand-in reply to: ")[1]) if "stand-in" in l else l for l in order]
    R.check(got == want, f"they are sent in order, each after the reply before it ends: {got}")
    R.check(not any(l.startswith("waiting: ") for l in chat_lines(desk, cid)), "no line is left waiting")

    # Esc clears the waiting lines; the reply in progress still ends.
    say("four")
    time.sleep(0.3)
    say("five")
    say("six")
    R.check(sum(l.startswith("waiting: ") for l in chat_lines(desk, cid)) == 2, "two lines wait behind four")
    desk.person_keys("Escape")
    time.sleep(0.4)
    d = desk.j("describe", id=cid)
    R.check(not any(l.startswith("waiting: ") for l in d["text"]) and "waiting" not in d["state"],
            "Esc clears the waiting lines")
    time.sleep(3.5)
    t = chat_lines(desk, cid)
    R.check(any("stand-in reply to: four" in l for l in t) and not any(l == "User: five" for l in t),
            "the reply in progress ends; the cleared lines are never sent")

    # Two asks over the socket while it is busy: both wait, neither replaces the other.
    say("seven")
    time.sleep(0.3)
    desk.ipc("wibwob_ask", text="from eri", source="guest", actor="eri")
    desk.ipc("wibwob_ask", text="from bot9", source="visitor", actor="bot9")
    time.sleep(0.3)
    w = [l for l in chat_lines(desk, cid) if l.startswith("waiting: ")]
    R.check(w == ["waiting: from eri", "waiting: from bot9"], f"two socket asks wait in order: {w}")
    done = wait_for(lambda: (lambda t: t if any("reply to: from bot9" in l for l in t) else None)(chat_lines(desk, cid)), 14)
    acts = [a["what"] for a in desk.j("acts", by="bot9")["acts"]]
    R.check(done and any("[from bot9 | source visitor" in a for a in acts),
            "each queued ask is sent with its own author's stamp")
    desk.ipc("close_window", id=cid)


def feed_lines(desk, fid):
    return desk.j("describe", id=fid)["text"]


def test_feed(desk):
    print("feed")
    desk.ipc("close_all", actor="setup")
    time.sleep(0.4)
    fid = desk.j("open_feed", x=0, y=26, w=150, h=20, actor="setup")["id"]
    w1 = desk.j("create_window", type="text_editor", x=2, y=1, w=40, h=10, title="one", actor="setup")["id"]
    w2 = desk.j("create_window", type="text_editor", x=50, y=1, w=40, h=10, title="two", actor="setup")["id"]
    time.sleep(0.3)

    # A resident waits on a dialog its own input opened in front of w1.
    j = desk.j("resident_menu", id=w1, choose="Help>About WIBWOBWORLD", actor="wib")
    time.sleep(0.8)
    k = desk.j("resident_key", id=w1, text="x", actor="wib")
    time.sleep(0.6)
    lines = feed_lines(desk, fid)
    R.check(any(" WIB " in l[:14] and "waiting: a dialog is in front of " + w1 in l for l in lines),
            f"a resident waiting on a dialog is a line: {[l for l in lines if 'waiting' in l][-1:]}")
    desk.ipc("resident_press", label="OK", actor="wib")
    desk.wait_seqs(k["first"], k["last"])
    time.sleep(0.4)
    lines = feed_lines(desk, fid)
    R.check(any(" WIB " in l[:14] and "stopped waiting after" in l for l in lines), "and so is the end of the wait, with its seconds")

    # Waiting for the person's mouse button.
    desk.person_mouse("down", 120, 15)
    k = desk.j("resident_key", id=w2, text="y", actor="wob")
    time.sleep(0.5)
    desk.person_mouse("up", 120, 15)
    desk.wait_seqs(k["first"], k["last"])
    time.sleep(0.3)
    lines = feed_lines(desk, fid)
    R.check(any(" WOB " in l[:14] and "waiting: the person's mouse button is down" in l for l in lines),
            "a resident waiting for the person's mouse button is a line")

    # Wib and Wob have different hands.
    time.sleep(1.2)
    a = desk.j("resident_key", id=w1, text="w", actor="wib")
    b = desk.j("resident_key", id=w2, text="b", actor="wob")
    desk.wait_seqs(a["first"], b["last"])
    hands = {h["actor"]: h for h in desk.j("resident_hand")["hands"]}
    screen = desk.screen()
    cell = lambda h: screen[h["y"]][h["x"]] if h["y"] < len(screen) and h["x"] < len(screen[h["y"]]) else ""
    R.check(hands["wib"]["letter"] == "W" and hands["wob"]["letter"] == "B",
            f"wib's hand is W and wob's is B ({hands['wib']['letter']}, {hands['wob']['letter']})")

    # A desk-mosh step (many shard acts by actor mosh) is one line.
    for i in range(4):
        r = desk.j("create_window", type="test_pattern", x=100 + i, y=2 + i, w=16, h=6, title=f"shard{i}", actor="mosh")
        desk.ipc("move_window", id=r["id"], x=104 + i, y=3 + i, actor="mosh")
    time.sleep(0.4)
    lines = feed_lines(desk, fid)
    mosh = [l for l in lines if "mosh" in l]
    R.check(len(mosh) == 1 and "mosh step:" in mosh[0], f"a mosh step folds into one line: {mosh}")
    desk.ipc("close_all", actor="setup")


def test_child(_desk):
    print("child")
    name = NAME + "_child"
    fake = f"/tmp/wibwob_{name}_fake"
    os.makedirs(fake, exist_ok=True)
    with open(f"{fake}/claude", "w") as f:
        f.write('#!/bin/sh\nif [ "$1" = "auth" ]; then echo \'{"loggedIn": true}\'; exit 0; fi\necho dying >&2\nexit 3\n')
    os.chmod(f"{fake}/claude", 0o755)
    desk = Desk(name, env={"PATH": f"{fake}:/usr/bin:/bin", "ANTHROPIC_API_KEY": ""}).start()
    try:
        fid = desk.j("open_feed", x=0, y=20, w=120, h=12, actor="setup")["id"]
        desk.ipc("open_scramble", actor="setup")
        time.sleep(0.5)
        desk.ipc("scramble_say", text="hello", actor="setup")
        got = wait_for(lambda: [l for l in feed_lines(desk, fid) if "model process ended with status" in l], 10)
        R.check(got and " SYS " in got[0][:14] and "child process died" in got[0],
                f"a child process that died is a system line: {got[:1]}")
    finally:
        R.check(desk.alive(), "the child desktop is still running")
        desk.stop()


def test_teach(desk):
    print("teach")
    desk.ipc("close_all", actor="setup")
    time.sleep(0.3)
    r = desk.ipc("opn_window")
    R.check("did you mean:" in r and "_window" in r.split("did you mean:")[1], f"a mistyped command suggests names: {r[:110]!r}")
    r = desk.ipc("exec_command", name="describ")
    R.check("did you mean: describe" in r, f"through exec_command too: {r[:80]!r}")
    r = desk.ipc("create_window", type="text editor")
    R.check("did you mean: text_editor" in r, f"a misnamed window type suggests the type: {r[:90]!r}")
    h = desk.j("help", name="resident_hand", actor="wib", source="resident")
    R.check(h["you_may"] is False and "console key" in h["why_not"] and "resident wib" in h["why_not"],
            f"help says why a rule is not this caller's: {h.get('why_not', '')[:120]!r}")
    h = desk.j("help", name="look_glance", actor="wib", source="resident")
    R.check(h["you_may"] is True and h["kind"] == "registry", "and that an ordinary command is")
    h = desk.j("help", name="create_window")
    R.check(h["kind"] == "socket" and "type=" in h["description"], "help covers the socket's own commands")
    all_ = desk.j("help")
    R.check("create_window" in all_["socket"] and "describe" in all_["registry"] and all_["rules"], "help lists every command and the rules")
    r = desk.ipc("help", name="describ")
    R.check("did you mean: describe" in r, "help on a mistyped name suggests too")

    # The resident's self-test, run by wob on itself.
    s = desk.j("resident_selftest", start=1, actor="wob", source="resident")
    rep = wait_for(lambda: (lambda x: x if not x["running"] else None)(desk.j("resident_selftest")), 40, 0.4)
    rep = rep or desk.j("resident_selftest")
    bad = [c for c in rep["checks"] if not c["ok"]]
    R.check(rep["actor"] == "wob" and len(rep["checks"]) >= 9 and not bad,
            f"wob's self-test: {rep['passed']} of {len(rep['checks'])} passed (failed: {bad[:2]})")
    acts = [a["what"] for a in desk.j("acts", by="wob")["acts"]]
    R.check(any(a.startswith("self-test: ") and "passed" in a for a in acts), "its result is an act in wob's name (the Feed shows it)")
    R.check(not any(w["title"] == "self-test" for w in desk.state()["windows"]), "it leaves no window behind")


def main():
    groups = [g.lower() for g in sys.argv[1:]] or ["chat", "feed", "child", "teach"]
    desk = Desk(NAME, env=NO_LLM).start()
    try:
        for g in groups:
            {"chat": test_chat, "feed": test_feed, "child": test_child, "teach": test_teach}[g](desk)
    except Exception as e:  # noqa: BLE001
        import traceback
        traceback.print_exc()
        R.check(False, f"exception: {e!r}")
    finally:
        R.check(desk.alive(), "the desktop is still running")
        desk.stop()
    print(f"\n{R.passed} passed, {R.failed} failed")
    sys.exit(1 if R.failed else 0)


if __name__ == "__main__":
    main()
