#!/usr/bin/env python3
"""Resident room tests: several minds in one room.

Starts its own desktops (tmux -L <ROOM_TEST_NAME>..., default roomtest) without a model: the claude CLI is
kept off PATH, and the Wib&Wob chat answers with a stand-in reply (WWDOS_CHAT_STANDIN_MS) so it is busy for
a known time. Binary: WWDOS_BIN or build-resident/app/wwdos.

Usage:
    python3 tests/resident/test_resident_room.py            # all groups
    python3 tests/resident/test_resident_room.py chat       # chosen groups
Groups: chat (typing while a reply streams).
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


def main():
    groups = [g.lower() for g in sys.argv[1:]] or ["chat"]
    desk = Desk(NAME, env=NO_LLM).start()
    try:
        for g in groups:
            {"chat": test_chat}[g](desk)
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
