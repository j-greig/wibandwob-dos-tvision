#!/usr/bin/env python3
"""Resident world tests: every window says what it is.

Starts its own desktop (tmux -L <name>, socket /tmp/wibwob_<name>.sock) without the claude CLI on PATH
and without ANTHROPIC_API_KEY, so no window talks to a model, and stops it after.
Binary: WWDOS_BIN or build-resident/app/wwdos.

Usage:
    python3 tests/resident/test_resident_world.py              # all groups
    python3 tests/resident/test_resident_world.py describe     # chosen groups
Groups: describe (the describe contract, the audit, every window type), wheel (the mouse wheel while a
resident's hand or reading mark is on screen).
"""

import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import REPO, Desk, Results  # noqa: E402

R = Results()
NO_LLM = {"PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "ANTHROPIC_API_KEY": ""}

# Window types that need arguments to open, and the ones the tests do not open.
OPEN_ARGS = {
    "frame_player": {"path": "app/frames_demo.txt"},
    "text_view": {"path": "app/sample_text.txt"},
    "figlet_text": {"text": "hello"},
}
NOT_OPENED = {
    "backrooms_tv": "opening one stops the app (unmodified build too)",
    "scramble": "not creatable by create_window; opened by scramble_toggle",
    "wibwob": "covered below with a message in it",
    "ascii_image": "opened from an image file only",
}


def new_window(desk, kind, **kv):
    """Open a window of a type; return its id (from the reply, else from the change in get_state)."""
    before = set(desk.windows())
    r = desk.ipc("create_window", type=kind, x=2, y=1, w=60, h=16, **kv)
    try:
        wid = json.loads(r).get("id")
        if wid:
            return wid, r
    except json.JSONDecodeError:
        pass
    time.sleep(0.3)
    after = set(desk.windows()) - before
    return (sorted(after)[0] if after else None), r


def describe(desk, wid, **kv):
    return desk.j("describe", id=wid, **kv)


def test_describe(desk):
    print("describe")
    # The static audit: every window class compiled into the app has a describer.
    a = subprocess.run([sys.executable, os.path.join(REPO, "tools", "describe_audit.py"), "--json"],
                       capture_output=True, text=True)
    audit = json.loads(a.stdout) if a.stdout.strip().startswith("{") else {"windows": [], "missing": ["?"]}
    R.check(a.returncode == 0 and not audit["missing"] and not audit["matcher_classes_missing"]
            and len(audit["windows"]) >= 40,
            f"the audit lists {len(audit['windows'])} window classes in the app and none lacks a describer "
            f"(missing: {audit['missing'][:5]})")

    r = desk.ipc("describe")
    R.check(r.startswith("{"), f"describe answers ({r[:60]!r})")
    if not r.startswith("{"):
        return

    # Every window type the app can open: open it, ask it what it is, close it.
    types = [t["type"] for t in desk.j("get_window_types")["window_types"] if t.get("spawnable")]
    ok, bad = [], []
    for kind in types:
        if kind in NOT_OPENED:
            continue
        wid, reply = new_window(desk, kind, **OPEN_ARGS.get(kind, {}))
        if not wid:
            bad.append(f"{kind}: did not open ({reply[:50]})")
            continue
        time.sleep(0.15)
        d = describe(desk, wid, limit=5)
        good = (d.get("contract") is True and d.get("type") == kind and d.get("state")
                and "characters drawn" not in d["state"] and d.get("text_from") in ("model", "cells")
                and d.get("standing") == "data")
        (ok if good else bad).append(kind if good else f"{kind}: {d.get('class')} {d.get('state')!r}")
        desk.ipc("close_window", id=wid)
        time.sleep(0.1)
    R.check(not bad and len(ok) >= 30,
            f"{len(ok)} window types opened and each says what it is from its own model, by contract "
            f"(failures: {bad[:4]})")
    skipped = sorted(set(types) & set(NOT_OPENED))
    print(f"    not opened here: {', '.join(f'{k} ({NOT_OPENED[k]})' for k in skipped)}")

    # The editor: the person types; describe gives the words and the cursor, with no screen reading.
    ed = desk.editor(4, 2, 50, 10, title="notes")
    desk.ipc("focus_window", id=ed)
    time.sleep(0.3)
    desk.person_type("hello world")
    desk.person_keys("Enter")
    time.sleep(0.05)
    desk.person_type("second")
    time.sleep(0.5)
    d = describe(desk, ed)
    R.check(d["text"] == ["hello world", "second"] and d["text_from"] == "model",
            f"the editor's text comes from its model: {d['text']}")
    R.check("2 lines" in d["state"] and "line 2, column 7" in d["state"] and '"hello world"' in d["state"],
            f"the editor's sentence carries its size, first line and cursor: {d['state']!r}")
    # Covered completely by another window: the text is the same.
    cover, _ = new_window(desk, "test_pattern")
    desk.ipc("move_window", id=cover, x=0, y=0)
    desk.ipc("resize_window", id=cover, width=80, height=20)
    time.sleep(0.3)
    d2 = describe(desk, ed)
    R.check(d2["seen"] == "none" and d2["text"] == ["hello world", "second"],
            "a window nobody can see still says what it holds")
    desk.ipc("close_window", id=cover)

    # window_text: any type whose model holds text, not only the editor.
    paint, _ = new_window(desk, "paint")
    desk.ipc("paint_text", id=paint, x=0, y=0, text="INK", fg=15, bg=0)
    time.sleep(0.2)
    wt = desk.ipc("window_text", id=paint)
    R.check(wt.startswith("{") and json.loads(wt)["lines"][:1] == ["INK"] and json.loads(wt)["type"] == "paint",
            f"window_text reads a paint canvas from its model ({wt[:60]!r})")
    desk.ipc("close_window", id=paint)

    # The chat: its messages are its text.
    chat, _ = new_window(desk, "wibwob")
    desk.ipc("exec_command", name="wibwob_ask", text="hello from a guest", source="guest", actor="eri")
    t0 = time.time()
    dc = describe(desk, chat)
    while not dc["text"] and time.time() - t0 < 8:
        time.sleep(0.3)
        dc = describe(desk, chat)
    said = [l for l in dc["text"] if not l.startswith("  ")]
    R.check(dc["contract"] and f"chat with {len(said)} messages" in dc["state"] and len(said) >= 1
            and all(": " in l for l in said),
            f"the chat says how many messages it holds and gives them as text: {dc['state']!r} {dc['text'][:4]}")
    desk.ipc("close_window", id=chat)

    # A dialog in front: described by its kind, its controls and its text.
    desk.ipc("resident_menu", choose="Help>About WIBWOBWORLD", actor="wib")
    time.sleep(1.0)
    alld = desk.j("describe")
    dlg = [w for w in alld["windows"] if w["class"] == "dialog"]
    R.check(dlg and "button" in dlg[0]["state"] and dlg[0]["contract"],
            f"a dialog in front is described by its controls: {dlg[0]['state'] if dlg else None!r}")
    if dlg:
        desk.ipc("resident_press", label="OK", actor="wib")
        time.sleep(0.6)

    # Every window on the desk, at once.
    desk.editor(10, 4, 30, 8, title="two")
    alld = desk.j("describe")
    R.check(alld["windows"] and all(w["contract"] for w in alld["windows"]),
            f"describe with no id lists {len(alld['windows'])} windows, each by contract")
    desk.ipc("close_all")
    time.sleep(0.3)


def wheel(desk, x, y, up=True):
    """The person turns the mouse wheel at screen cell x, y (xterm SGR buttons 64 and 65)."""
    subprocess.run(["tmux", "-L", desk.name, "send-keys", "-t", desk.name, "-l",
                    f"\x1b[<{64 if up else 65};{x + 1};{y + 1}M"], stderr=subprocess.DEVNULL)


def test_wheel(desk):
    print("wheel")
    # A resident's hand and reading mark are one-cell views in front of the desktop. A wheel event
    # reaches every view in the application group; the cell used to pass it back to the application,
    # which handed it to every view again, the cell included, until the stack ran out (SIGSEGV).
    ed = desk.editor(4, 2, 40, 10, title="wheel")
    other = desk.editor(50, 2, 40, 10, title="hand")
    desk.ipc("look_read", id=ed, reader="wob")                      # a reading mark on ed
    r = desk.j("resident_key", id=other, text="x", actor="wib")     # wib's hand on other
    desk.wait_seqs(r["first"], r["last"])
    hands = [h for h in desk.j("resident_hand")["hands"] if h["visible"]]
    marks = desk.j("look_mark")["marks"]
    R.check(hands and marks, f"a hand ({len(hands)}) and a reading mark ({len(marks)}) are on screen")
    hx, hy = hands[0]["x"], hands[0]["y"]
    for k in range(6):
        wheel(desk, 20, 6, up=k % 2 == 0)          # over a window
        wheel(desk, hx, hy)                        # over the hand itself
        time.sleep(0.02)
    time.sleep(0.5)
    R.check(desk.alive() and desk.ipc("get_canvas_size").startswith("{"),
            "the desktop survives the person turning the wheel while a hand and a mark are up")
    # A click on the hand still reaches the window underneath (after share_ms: the wheel was the person's act).
    time.sleep(1.7)
    r = desk.j("resident_key", id=other, text="y", actor="wib")
    desk.wait_seqs(r["first"], r["last"])
    h = [h for h in desk.j("resident_hand")["hands"] if h["visible"]]
    if h and desk.alive():
        desk.person_mouse("down", h[0]["x"], h[0]["y"])
        desk.person_mouse("up", h[0]["x"], h[0]["y"])
        time.sleep(0.3)
    R.check(desk.alive() and desk.focused() == other,
            "a click on the hand goes to the window underneath it")
    desk.ipc("close_all")
    time.sleep(0.3)


def main():
    groups = [g.lower() for g in sys.argv[1:]] or ["describe", "wheel"]
    desk = Desk(os.environ.get("WORLD_TEST_NAME", "worldtest"), env=NO_LLM).start()
    try:
        for g in groups:
            {"describe": test_describe, "wheel": test_wheel}[g](desk)
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
