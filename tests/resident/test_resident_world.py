#!/usr/bin/env python3
"""Resident world tests: every window says what it is.

Starts its own desktop (tmux -L <name>, socket /tmp/wibwob_<name>.sock) without the claude CLI on PATH
and without ANTHROPIC_API_KEY, so no window talks to a model, and stops it after.
Binary: WWDOS_BIN or build-resident/app/wwdos.

Usage:
    python3 tests/resident/test_resident_world.py              # all groups
    python3 tests/resident/test_resident_world.py describe     # chosen groups
Groups: describe (the describe contract, the audit, every window type), wheel (the mouse wheel while a
resident's hand or reading mark is on screen), gestures (the person's acts as a stream a resident waits on),
hands (residents inside the app: reflexes, Scramble's paws, one command table; its own desktop with a
stand-in claude CLI that answers with canned text).
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


def wait_gesture(desk, since, ms=6000, kinds=None):
    """person_wait in a thread; returns (thread, box) where box gets (reply, seconds) when it answers."""
    import threading
    box = {}

    def run():
        t0 = time.time()
        kv = {"since": since, "ms": ms}
        if kinds:
            kv["kinds"] = kinds
        r = desk.ipc("person_wait", timeout=ms / 1000 + 5, **kv)
        box["t"] = time.time()
        box["dt"] = box["t"] - t0
        box["r"] = json.loads(r) if r.startswith("{") else {"error": r}

    th = threading.Thread(target=run)
    th.start()
    return th, box


def test_gestures(desk):
    print("gestures")
    desk.ipc("close_all", actor="setup")
    time.sleep(0.5)
    r = desk.ipc("person_events", since=0)
    R.check(r.startswith("{"), f"person_events answers ({r[:50]!r})")
    if not r.startswith("{"):
        return
    ed = desk.editor(4, 2, 40, 10, title="mine")
    mine = desk.editor(60, 2, 40, 10, title="wibs")
    desk.ipc("focus_window", id=ed)
    time.sleep(0.4)
    g0 = desk.j("person_events", since=0)["last"]

    # A resident waits; the person types one key; the wait answers at once, with no polling.
    th, box = wait_gesture(desk, g0)
    time.sleep(0.8)
    R.check("r" not in box, "person_wait holds the reply while the person does nothing")
    t_key = time.time()
    desk.person_text("h")
    th.join(8)
    got = box.get("r", {})
    gs = got.get("gestures", [])
    lag = box.get("t", 99) - t_key
    R.check(gs and gs[0]["kind"] == "typed" and gs[0]["window"] == ed and gs[0]["type"] == "text_editor"
            and gs[0]["text"] == "h" and lag < 0.5,
            f"the wait answers {lag * 1000:.0f} ms after the person's key with the gesture: {gs[:1]}")
    # More typing: the same act comes again as an update with the whole run.
    desk.person_type("ello")
    time.sleep(0.3)
    ev = desk.j("person_events", since=gs[0]["g"] if gs else g0)["gestures"]
    R.check(ev and ev[-1]["update"] and ev[-1]["text"] == "hello" and ev[-1]["act"] == gs[0]["act"],
            f"typing on in the same window updates the gesture: {ev[-1:] if ev else ev}")

    # The resident acts (types into its own window, opens and moves a window): the person's stream stays quiet.
    g1 = desk.j("person_events", since=0)["last"]
    th, box = wait_gesture(desk, g1, ms=2500)
    r = desk.j("resident_key", id=mine, text="from wib", actor="wib")
    desk.wait_seqs(r["first"], r["last"])
    j = desk.j("resident_menu", choose="Window>Text Editor", actor="wib")
    desk.wait_seqs(j["first"], j["last"])
    time.sleep(0.3)
    opened = [w for w in desk.state()["windows"] if w["id"] not in (ed, mine) and w["type"] == "text_editor"]
    if opened:
        desk.ipc("move_window", id=opened[0]["id"], x=30, y=12, actor="wib", source="resident")
    th.join(6)
    got = box.get("r", {})
    acts = desk.j("acts", since=0, by="wib")["acts"]
    R.check(got.get("timeout") is True and got.get("count") == 0 and len(acts) >= 10,
            f"none of wib's {len(acts)} acts (keys, a menu command, an open, a move) reaches the person's stream")

    # The person opens a menu, chooses an item (a new window), drags it and closes it.
    g2 = desk.j("person_events", since=0)["last"]
    desk.person_keys("M-w")
    time.sleep(0.5)
    mid = desk.j("person_events", since=g2)["gestures"]
    desk.person_keys("Escape")
    time.sleep(0.3)
    desk.person_keys("C-n")
    time.sleep(0.6)
    pats = [w for w in desk.state()["windows"] if w["type"] == "test_pattern"]
    pat = pats[0] if pats else None
    if pat:
        x0, y0 = pat["x"] + 8, pat["y"] + 1
        desk.person_mouse("down", x0, y0)
        for k in range(1, 6):
            desk.person_mouse("drag", x0 + k, y0 + k)
            time.sleep(0.03)
        desk.person_mouse("up", x0 + 5, y0 + 5)
        time.sleep(0.4)
        desk.person_keys("M-w")                                   # Window > Close (the dragged window is current)
        time.sleep(0.4)
        desk.person_keys("e")
        time.sleep(0.5)
    ev = desk.j("person_events", since=g2)["gestures"]
    kinds = [g["kind"] for g in ev]
    R.check(any(g["kind"] == "menu" and g["what"].startswith("opened menu") for g in mid),
            f"opening a menu is a gesture: {[g['what'] for g in mid]}")
    R.check(pat and any(g["kind"] == "opened" and g["window"] == pat["id"] and g["type"] == "test_pattern" for g in ev),
            "the window the person opened is a gesture with its type")
    R.check(pat and any(g["kind"] in ("drag", "moved") and g["window"] == pat["id"] for g in ev),
            "dragging it is a gesture")
    R.check(pat and any(g["kind"] == "closed" and g["window"] == pat["id"] and g["type"] == "test_pattern" for g in ev),
            f"closing it is a gesture that still names its type: {kinds}")
    nums = [g["g"] for g in ev]
    R.check(nums == sorted(nums) and len(set(nums)) == len(nums), f"gestures are numbered in order ({len(nums)})")
    person_acts = {a["n"] for a in desk.j("acts", since=0, by="person")["acts"]}
    R.check(all(g["act"] in person_acts for g in ev), "every gesture is an act of the person in the act log")

    # Pushed: a subscriber receives the same gestures as "gesture" events.
    import socket as so
    s = so.socket(so.AF_UNIX, so.SOCK_STREAM)
    s.settimeout(3)
    s.connect(desk.sock)
    s.sendall(b"cmd:subscribe_events\n")
    buf = s.recv(4096)
    desk.ipc("focus_window", id=ed)
    time.sleep(0.3)
    desk.person_text("!")
    t0 = time.time()
    pushed = []
    while time.time() - t0 < 3 and not pushed:
        try:
            buf += s.recv(65536)
        except so.timeout:
            break
        for line in buf.decode("utf-8", "replace").split("\n"):
            if '"event":"gesture"' in line:
                pushed.append(json.loads(line)["payload"])
    s.close()
    R.check(pushed and pushed[0]["window"] == ed and pushed[0]["actor"] == "person",
            f"subscribe_events pushes the person's gesture: {pushed[:1]}")
    desk.ipc("close_all")
    time.sleep(0.3)


FAKE_CLAUDE = r"""#!/bin/sh
# A stand-in for the claude CLI: says it is logged in, logs each call, answers with a canned reply.
if [ "$1" = "auth" ]; then echo '{"loggedIn": true, "email": "test@example.invalid", "authMethod": "test"}'; exit 0; fi
for a in "$@"; do printf '%s\n' "<$a>"; done > "$FAKE_DIR/args.txt"
cat "$FAKE_DIR/reply.txt"
"""

REFLEXES = '''# Residents inside the app answer the person's gestures (docs/resident-world.md).
start as wib -> notes: create_window type=text_editor x=100 y=1 w=55 h=22 title=residents
on opened as wib: resident_key id={notes} text="wib: you opened a {type}\\n"
on opened as wob: look_read id={window} reader=wob
on closed as wob: resident_key id={notes} text="wob: the {type} is closed\\n"
on typed type=text_editor as scramble: resident_key id={notes} text="scramble: you wrote {text}\\n"
'''


def wait_text(desk, wid, needle, timeout=6.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        r = desk.ipc("window_text", id=wid)
        if r.startswith("{") and any(needle in l for l in json.loads(r)["lines"]):
            return True
        time.sleep(0.15)
    return False


def test_hands(_desk):
    print("hands")
    name = os.environ.get("WORLD_TEST_NAME", "worldtest") + "_hands"
    fake = f"/tmp/wibwob_{name}_fake"
    os.makedirs(fake, exist_ok=True)
    with open(f"{fake}/claude", "w") as f:
        f.write(FAKE_CLAUDE)
    os.chmod(f"{fake}/claude", 0o755)
    with open(f"{fake}/reply.txt", "w") as f:
        f.write("nothing yet\n")
    script = f"/tmp/wibwob_{name}.reflex"
    with open(script, "w") as f:
        f.write(REFLEXES)
    env = {"PATH": f"{fake}:/usr/bin:/bin:/usr/sbin:/sbin", "ANTHROPIC_API_KEY": "", "FAKE_DIR": fake,
           "WWDOS_RESIDENTS": script}
    desk = Desk(name, env=env).start()
    try:
        time.sleep(1.0)
        # One command table: a registry-only command answers by its own name on the socket.
        rf = desk.ipc("resident_reflexes")
        R.check(rf.startswith("{"), f"registry commands answer on the socket by name ({rf[:40]!r})")
        if not rf.startswith("{"):
            return
        rf = json.loads(rf)
        notes = rf["bound"].get("notes")
        own = desk.j("window_owner", id=notes) if notes else {}
        R.check(notes and own.get("owner") == "wib", f"the reflex script's start line: wib opened its notes window {notes}")
        t = desk.j("create_window", type="test_pattern", x=1, y=1, w=20, h=6, actor="setup")["id"]
        desk.ipc("window_title", id=t, title="renamed", actor="setup")
        time.sleep(0.2)
        R.check(desk.windows()[t]["title"] == "renamed", "window_title (once reachable only through exec_command) works by name")
        desk.ipc("close_window", id=t, actor="setup")
        R.check(desk.ipc("resident_reflexes", load=script).startswith("err refused"),
                "loading a reflex script from the socket is a rule, refused")

        # The person opens a window: wib answers in its notes with its hand, wob reads the new window.
        a0 = desk.j("acts", limit=1)["next"]
        desk.person_keys("C-n")
        ok = wait_text(desk, notes, "wib: you opened a test_pattern")
        pat = [w for w in desk.state()["windows"] if w["type"] == "test_pattern"]
        hands = {h["actor"]: h for h in desk.j("resident_hand")["hands"]}
        marks = desk.j("look_mark")["marks"]
        R.check(ok, "the person opens a test pattern; wib writes in its notes, through the resident path")
        R.check(pat and desk.focused() == pat[0]["id"], "the residents' answer leaves the person's focus on the window they opened")
        R.check("wib" in hands and hands["wib"]["steps"] > 0 and hands["wib"]["arrived_ms"] >= 0,
                f"wib's hand glided to its notes before the keys landed ({hands.get('wib')})")
        R.check(pat and any(m["reader"] == "wob" and m["window"] == pat[0]["id"] for m in marks),
                f"wob looks at the window the person opened (its reading mark: {marks})")
        # The person types in an editor of their own: Scramble answers when the typing pauses.
        ed = desk.j("create_window", type="text_editor", x=2, y=10, w=40, h=8, title="mine", actor="setup")["id"]
        w = desk.windows()[ed]
        desk.person_mouse("down", w["x"] + 5, w["y"] + 3)
        desk.person_mouse("up", w["x"] + 5, w["y"] + 3)
        time.sleep(0.3)
        desk.person_type("hi there")
        R.check(wait_text(desk, notes, "scramble: you wrote hi there"),
                "the person types; when the typing pauses Scramble writes what they wrote, with her own hand")
        hands = {h["actor"]: h for h in desk.j("resident_hand")["hands"]}
        R.check("scramble" in hands, "Scramble has a hand of her own")
        # The person closes the pattern: wob notes it.
        if pat:
            desk.ipc("focus_window", id=pat[0]["id"])
            time.sleep(1.6)
            desk.person_keys("M-w")
            time.sleep(0.4)
            desk.person_keys("e")
        R.check(wait_text(desk, notes, "wob: the test_pattern is closed"), "the person closes it; wob writes that down")
        acts = desk.j("acts", since=a0)["acts"]
        res = [a for a in acts if a["source"] == "resident"]
        R.check({a["actor"] for a in res} >= {"wib", "wob", "scramble"} and
                not any(a["actor"] == "person" and a["window"] == notes for a in acts),
                f"every answer is an act by wib, wob or scramble ({len(res)} acts); none is the person's")
        fired = desk.j("resident_reflexes")["fired"]
        time.sleep(1.0)
        R.check(desk.j("resident_reflexes")["fired"] == fired, "the residents' own acts never set off a reflex")

        # Scramble's paws: her model's ACT lines act on the desktop as actor scramble; she sees only the desktop.
        desk.ipc("open_scramble", actor="setup")
        time.sleep(0.5)
        with open(f"{fake}/reply.txt", "w") as f:
            f.write(f'done. it says paw now. (=^..^=)\nACT: resident_key id={notes} text="paw\\n"\n'
                    f'ACT: quit_app\n')
        desk.ipc("scramble_say", text="which windows are open? and write paw in the notes", source="guest", actor="eri")
        R.check(wait_text(desk, notes, "paw", 8), "Scramble's ACT line typed into the notes window")
        args = open(f"{fake}/args.txt").read() if os.path.exists(f"{fake}/args.txt") else ""
        R.check("<--tools>\n<>" in args and "<--strict-mcp-config>" in args,
                "her model call has no tools and no MCP servers (it cannot touch the person's machine)")
        R.check("THE DESKTOP" in args and f"- {notes} text_editor" in args and "[from eri | source guest" in args,
                "her prompt carries the desktop's own windows and who is asking (eri, a guest)")
        sacts = [a for a in desk.j("acts", by="scramble")["acts"]]
        R.check(any(a["what"].startswith("key ") and a["window"] == notes for a in sacts) and desk.alive(),
                "her keys are logged as hers; a command outside the paws (quit_app) is refused and the desk runs on")
        # "open an editor and type hello" in one reply: id=new names the window her last act opened.
        with open(f"{fake}/reply.txt", "w") as f:
            f.write('ACT: create_window type=text_editor x=2 y=20 w=30 h=6 title=paws\nACT: resident_key id=new text=hello\nthere.\n')
        time.sleep(2.5)
        desk.ipc("scramble_say", text="open an editor and type hello", actor="setup")
        t0 = time.time()
        paws = []
        while time.time() - t0 < 8 and not paws:
            paws = [w for w in desk.state()["windows"] if w["title"] == "paws"]
            time.sleep(0.2)
        R.check(paws and wait_text(desk, paws[0]["id"], "hello") and desk.j("window_owner", id=paws[0]["id"])["owner"] == "scramble",
                "Scramble opens an editor and types hello in it, in one reply; the window is hers")
        sh = desk.j("describe", id=[w for w in desk.state()["windows"] if w["type"] == "scramble"][0]["id"])
        R.check(not any("ACT:" in l for l in sh["text"]), "the ACT lines are not shown in her chat")
        # The chat's tools reach this socket directly as wob (the bridge's client, no HTTP server, no npm packages).
        node = subprocess.run(["node", os.path.join(REPO, "app/llm/sdk_bridge/app_socket.js"), "wob", "resident_key",
                               f"id={notes}", "text=from the chat"], capture_output=True, text=True,
                              env={**os.environ, "WWDOS_SOCKET": desk.sock})
        R.check(node.returncode == 0 and wait_text(desk, notes, "from the chat")
                and any(a["actor"] == "wob" and a["source"] == "resident" and a["window"] == notes
                        for a in desk.j("acts", by="wob")["acts"]),
                f"the chat's tool client acts on the desktop's own socket as wob ({node.stdout.strip()[:50]!r})")
        # The Feed: the act log, live, one line per act with a tag for who acted.
        f = desk.j("open_feed", x=0, y=24, w=110, h=14, actor="setup")
        fid = f.get("id")
        time.sleep(0.6)
        fd = desk.j("describe", id=fid)
        lines = fd["text"]
        tags = {t: [l for l in lines if f" {t} " in l[:14]] for t in ("PER", "WIB", "WOB", "SCR", "VIS", "SYS")}
        R.check(fd["contract"] and all(tags.values()),
                f"the feed shows every kind of actor by its tag: {({t: len(v) for t, v in tags.items()})}")
        R.check(any("model call finished after" in l for l in tags["SYS"]),
                f"system events are in it: {[l for l in tags['SYS'] if 'model' in l][:2]}")
        R.check(any("refused" in l for l in lines), "a refused act carries its reason")
        R.check(any(l[8:13] == " PER " and "typed hi there" in l for l in lines),
                "the person's typing is one line, tagged PER")
        n1 = desk.j("acts", limit=1)["next"]
        time.sleep(1.5)
        R.check(desk.j("acts", limit=1)["next"] == n1, "the feed's own redraws are not acts")
        desk.ipc("resident_key", id=notes, text="!", actor="wob")
        time.sleep(1.0)
        R.check(desk.j("describe", id=fid)["text"][-1:] and "WOB" in desk.j("describe", id=fid)["text"][-1][:14]
                or any("WOB" in l[:14] and "key !" in l for l in desk.j("describe", id=fid)["text"][-6:]),
                "a new act appears at the bottom of the open feed")
        w2 = desk.j("open_feed", by="wib", actor="setup").get("id")
        time.sleep(0.4)
        only = desk.j("describe", id=w2)
        R.check(only["text"] and all(" WIB " in l[:14] for l in only["text"]) and "only WIB" in only["state"],
                f"a feed filtered by actor shows only that actor ({len(only['text'])} lines)")
    finally:
        R.check(desk.alive(), "the hands desktop is still running")
        desk.stop()


def main():
    groups = [g.lower() for g in sys.argv[1:]] or ["describe", "wheel", "gestures", "hands"]
    desk = Desk(os.environ.get("WORLD_TEST_NAME", "worldtest"), env=NO_LLM).start()
    try:
        for g in groups:
            {"describe": test_describe, "wheel": test_wheel, "gestures": test_gestures, "hands": test_hands}[g](desk)
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
