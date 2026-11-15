#!/usr/bin/env python3
"""Residents' self-test with real model calls: each in-window resident gets one instruction over the
socket and the desktop is checked for the result. Opt-in, because it calls models (haiku, a few cents):

    WWDOS_SELFTEST_MODELS=1 python3 tests/resident/selftest_residents.py

Starts its own desktop (tmux -L selftest) with the claude CLI on PATH and WWDOS_CHAT_MODEL=haiku, then:
  scramble  "open a text editor and type hello"  -> an editor she owns holds hello
            "which windows are open?"           -> her reply names a window that is on the desktop
  wibwob    the same two, to the Wib&Wob chat    -> an editor owned by wib or wob holds hello; the reply
                                                    names a window that is on the desktop
Each instruction is sent as a visitor (actor selftest), so it carries that source and standing.
Prints PASS or FAIL per resident and check; exits 1 if any failed. Needs the chat bridge's packages
(npm install --prefix app/llm/sdk_bridge).
"""

import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import REPO, Desk  # noqa: E402

results = []


def check(who, ok, what):
    results.append((who, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {who}: {what}")


def editors_with(desk, text, owners):
    out = []
    for w in desk.state()["windows"]:
        if w["type"] != "text_editor":
            continue
        r = desk.ipc("window_text", id=w["id"])
        if r.startswith("{") and any(text in l.lower() for l in json.loads(r)["lines"]):
            if desk.j("window_owner", id=w["id"])["owner"] in owners:
                out.append(w["id"])
    return out


def wait(pred, timeout):
    t0 = time.time()
    while time.time() - t0 < timeout:
        v = pred()
        if v:
            return v
        time.sleep(1.0)
    return None


def describe(desk, wid):
    """describe, read again if a reply comes back empty (a slow moment while a model call starts)."""
    for _ in range(5):
        r = desk.ipc("describe", id=wid, timeout=10)
        if r.startswith("{"):
            return json.loads(r)
        time.sleep(1.0)
    raise AssertionError(f"describe {wid}: no answer (desktop alive: {desk.alive()})")


def reply_lines(desk, wid, after):
    return describe(desk, wid)["text"][after:]


def names_a_window(desk, lines):
    text = " ".join(lines).lower()
    present = desk.state()["windows"]
    return any(w["id"].lower() in text or w["type"].replace("_", " ") in text or w["type"] in text for w in present)


def main():
    if os.environ.get("WWDOS_SELFTEST_MODELS") != "1":
        print("skipped: set WWDOS_SELFTEST_MODELS=1 to run (it calls models)")
        return 0
    desk = Desk("selftest", env={"WWDOS_CHAT_MODEL": "haiku"}).start()
    try:
        # Scramble
        desk.ipc("open_scramble", actor="selftest")
        time.sleep(0.8)
        sid = [w for w in desk.state()["windows"] if w["type"] == "scramble"][0]["id"]
        desk.ipc("scramble_say", text="open a text editor and type hello in it", source="visitor", actor="selftest")
        got = wait(lambda: editors_with(desk, "hello", {"scramble"}), 60)
        check("scramble", bool(got), f"opened an editor and typed hello ({got})")
        n0 = len(describe(desk, sid)["text"])
        time.sleep(3)
        desk.ipc("scramble_say", text="which windows are open?", source="visitor", actor="selftest")
        lines = wait(lambda: [l for l in reply_lines(desk, sid, n0 + 1) if l.startswith("scramble:")], 60) or []
        check("scramble", names_a_window(desk, lines), f"named the desktop's windows: {' '.join(lines)[:120]!r}")

        # The Wib&Wob chat
        if not os.path.exists(os.path.join(REPO, "app/llm/sdk_bridge/node_modules/@anthropic-ai/claude-agent-sdk")):
            check("wibwob", False, "the chat bridge is not installed: npm install --prefix app/llm/sdk_bridge")
        else:
            cid = desk.j("create_window", type="wibwob", x=2, y=1, w=70, h=20, actor="selftest").get("id")
            time.sleep(1.0)
            before = set(editors_with(desk, "hello", {"wib", "wob"}))
            desk.ipc("wibwob_ask", text="open a text editor and type hello in it", source="visitor", actor="selftest")
            got = wait(lambda: set(editors_with(desk, "hello", {"wib", "wob"})) - before, 150)
            check("wibwob", bool(got), f"opened an editor and typed hello as wib or wob ({sorted(got or [])})")
            # let the first reply finish, then read only what comes after the second question
            wait(lambda: not describe(desk, cid)["state"].endswith("arriving") or None, 60)
            time.sleep(3)
            desk.ipc("wibwob_ask", text="which windows are open? answer in one line", source="visitor", actor="selftest")

            def answer():
                d = describe(desk, cid)
                t = d["text"]
                q = max((i for i, l in enumerate(t) if "which windows are open" in l), default=None)
                if q is None or d["state"].endswith("arriving"):
                    return None
                after = [l for l in t[q + 1:] if l.strip() and not l.startswith("System")]
                return after or None
            lines = wait(answer, 150) or []
            check("wibwob", names_a_window(desk, lines), f"named the desktop's windows: {' '.join(lines)[:120]!r}")
        # The Feed: every resident's acts and the model calls, read back from the window's own text.
        fid = desk.j("open_feed", x=0, y=26, w=120, h=14, actor="selftest").get("id")
        time.sleep(1.0)
        lines = describe(desk, fid)["text"]
        has = lambda tag: any(f" {tag} " in l[:14] for l in lines)
        check("feed", has("SCR") and has("VIS") and has("SYS") and (has("WIB") or has("WOB")),
              f"the feed shows Scramble, the chat (WIB/WOB), the visitor and system lines ({len(lines)} lines)")
        check("feed", any("model call finished after" in l for l in lines), "model calls appear with their seconds")
    finally:
        desk.stop()
    # Listening: the person types a request into an ordinary editor; a resident answers under it.
    ldesk = Desk("selftest_listen", env={"WWDOS_LISTEN": "1"}).start()
    try:
        ed = ldesk.j("create_window", type="text_editor", x=2, y=1, w=70, h=14, title="mine", actor="selftest")["id"]
        ldesk.ipc("focus_window", id=ed)
        time.sleep(0.5)
        ldesk.person_type("could you draw a small house for me?")
        ldesk.person_keys("Enter")

        def answered():
            r = ldesk.ipc("window_text", id=ed)
            lines = json.loads(r)["lines"] if r.startswith("{") else []
            return [l for l in lines if l.startswith(("wib:", "wob:", "scramble:"))] or None
        got = wait(answered, 90)
        notes = [a["what"] for a in ldesk.j("acts", by="app")["acts"]]
        check("listen", bool(got), f"a resident answered under the person's line: {(got or [''])[0][:80]!r}")
        check("listen", any("model call finished after" in n for n in notes) or any("answered as" in n for n in notes),
              "the Feed shows the call and why it answered")
    finally:
        ldesk.stop()

    for who in ("scramble", "wibwob", "feed", "listen"):
        mine = [ok for w, ok in results if w == who]
        print(f"{who}: {'PASS' if mine and all(mine) else 'FAIL'} ({sum(mine)}/{len(mine)})")
    return 0 if all(ok for _, ok in results) else 1


if __name__ == "__main__":
    sys.exit(main())
