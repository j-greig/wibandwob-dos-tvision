#!/usr/bin/env python3
"""The machine's sound: the synth, the ears and event scores, on a desktop of its own.

Starts its own desktop (tmux -L soundtest, socket /tmp/wibwob_soundtest.sock) with the synth's
sound off (WWDOS_SYNTH_AUDIO=0) and the TR-808's off, without the claude CLI on PATH, and stops
it after. Binary: WWDOS_BIN or build-resident/app/wwdos.

Usage:
    python3 tests/resident/test_machine_sound.py            # all groups
    python3 tests/resident/test_machine_sound.py synth      # chosen groups
Groups: synth.
"""

import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import Desk, Results  # noqa: E402

R = Results()
QUIET = {"PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "ANTHROPIC_API_KEY": "", "WWDOS_SYNTH_AUDIO": "0"}


def q(desk, cmd, **kv):
    """Queue a resident command and wait for every item it queued."""
    r = desk.ipc(cmd, **kv)
    if not r.startswith("{"):
        raise AssertionError(f"{cmd} {kv} -> {r!r}")
    j = json.loads(r)
    if j.get("ok"):
        desk.wait_seqs(j["first"], j["last"])
    return j


def x(desk, name, **kv):
    """A registry command (the sound commands are reached through exec_command)."""
    return desk.j("exec_command", name=name, **kv)


def xraw(desk, name, **kv):
    return desk.ipc("exec_command", name=name, **kv)


def last_act(desk):
    acts = desk.j("acts", since=0, limit=4000)["acts"]
    return acts[-1]["n"] if acts else 0


def acts_since(desk, n):
    return desk.j("acts", since=n, limit=4000)["acts"]


def glance_of(desk, wid):
    for w in desk.j("look_glance")["windows"]:
        if w["id"] == wid:
            return w
    return None


# ---------------------------------------------------------------------------- synth

def test_synth(desk):
    print("\n[synth] PC speaker and FM voices, played from the keyboard, by command and by a resident")
    desk.ipc("close_all")
    time.sleep(0.3)
    n0 = last_act(desk)
    o = x(desk, "open_synth", x=4, y=2, w=60, h=18, actor="wib", source="resident")
    wid = o.get("id")
    R.check(bool(wid), f"open_synth opens a window ({o})")
    g = glance_of(desk, wid) or {}
    R.check(g.get("type") == "synth", f"look_glance names its type synth ({g.get('type')})")
    R.check(g.get("state", "").startswith("synth, FM voice with patch bass"), f"look_glance gives one sentence: {g.get('state')!r}")
    R.check(g.get("owner") == "wib", f"the window is wib's, who opened it ({g.get('owner')})")
    R.check(any("SYNTH  voice FM (bass)" in line for line in desk.screen()), "the window shows its voice on screen")

    # A resident plays a phrase by command.
    s = x(desk, "synth", id=wid, action="notes", notes="C-4 E-4 G-4", step=60, actor="wib", source="resident")
    R.check(s.get("played") == 3 and [r["note"] for r in s["recent"][:3]] == ["G-4", "E-4", "C-4"],
            f"a phrase of three notes is played ({s.get('played')} played)")
    R.check(all(r["who"] == "wib" for r in s["recent"][:3]), "each note says wib played it")
    R.check(s.get("audio") is False and s.get("sent") == 0 and s.get("silenced") == 3,
            "with WWDOS_SYNTH_AUDIO=0 nothing reaches the speakers (sent 0, silenced 3)")

    # The person plays from the keyboard (tracker layout: z x c = C D E in octave 4).
    desk.person_type("zxc", gap=0.08)
    time.sleep(0.3)
    s = x(desk, "synth", id=wid, action="state")
    mine = [r for r in s["recent"] if r["who"] == "person"]
    R.check([r["note"] for r in mine[:3]] == ["E-4", "D-4", "C-4"], f"the person plays C D E from z x c ({[r['note'] for r in mine]})")

    # Tab switches to the PC speaker; ] raises the octave; q is then C-6.
    desk.person_keys("Tab")
    time.sleep(0.15)
    desk.person_text("]")
    time.sleep(0.15)
    desk.person_text("q")
    time.sleep(0.3)
    s = x(desk, "synth", id=wid, action="state")
    R.check(s["voice"] == "pcspk" and s["octave"] == 5, f"Tab chooses the PC speaker, ] octave 5 ({s['voice']}, {s['octave']})")
    R.check(s["recent"][0]["note"] == "C-6" and s["recent"][0]["voice"] == "pcspk", f"q plays C-6 on the PC speaker ({s['recent'][0]})")

    # Another resident plays through its own keys, without taking the person's focus.
    time.sleep(1.6)   # the window stays the person's for share_ms after their last key
    q(desk, "resident_key", id=wid, text="v", actor="wob")
    time.sleep(0.2)
    s = x(desk, "synth", id=wid, action="state")
    R.check(s["recent"][0]["who"] == "wob" and s["recent"][0]["note"] == "F-5", f"wob's key v plays F-5 as wob ({s['recent'][0]})")

    # The acts log names every author.
    acts = acts_since(desk, n0)
    by = lambda a, word: any(x["actor"] == a and word in x["what"] for x in acts)
    R.check(by("wib", "open_synth") and by("wib", "synth action=notes"),
            "the log has wib opening the synth and playing the phrase " + str([x["what"] for x in acts if x["actor"] == "wib"]))
    R.check(any(x["actor"] == "person" and x["source"] == "console" and x["window"] == wid for x in acts),
            "the log has the person's keys in the synth window")
    R.check(by("wob", "key"), "the log has wob's key")

    # Rendering to a buffer is deterministic and makes no sound.
    r1 = x(desk, "synth", id=wid, action="render", notes="A-4", voice="pcspk", dur=1000, ms=1000)
    r2 = x(desk, "synth", id=wid, action="render", notes="A-4", voice="pcspk", dur=1000, ms=1000)
    R.check(r1["samples"] == 22050 and 438 <= r1["up_crossings"] <= 441,
            f"render: PC speaker A-4 for a second is 22050 samples crossing zero {r1['up_crossings']} times")
    R.check(r1["fnv"] == r2["fnv"], f"render twice gives the same samples ({r1['fnv']})")
    f1 = x(desk, "synth", id=wid, action="render", notes="C-3 G-3 C-4 G-3", voice="fm", patch="bass", step=125)
    f2 = x(desk, "synth", id=wid, action="render", notes="C-3 G-3 C-4 G-3", voice="fm", patch="bell", step=125)
    R.check(f1["fnv"] != f2["fnv"] and f1["rms"] > 0.01, "render: an FM bassline differs by patch and has level")
    s = x(desk, "synth", id=wid, action="state")
    R.check(s["played"] == 8, f"render plays nothing ({s['played']} notes played in all)")
    R.check(xraw(desk, "synth", id=wid, action="note", note="H-9").startswith("err"), "a bad note is refused with err")


GROUPS = {"synth": test_synth}


def main():
    groups = [g.lower() for g in sys.argv[1:]] or list(GROUPS)
    desk = Desk(os.environ.get("SOUND_TEST_NAME", "soundtest"), env=QUIET).start()
    try:
        for g in groups:
            GROUPS[g](desk)
    finally:
        alive = desk.alive()
        desk.stop()
    print(f"\n{R.passed} passed, {R.failed} failed" + ("" if alive else " (the desktop stopped during the test)"))
    sys.exit(1 if R.failed or not alive else 0)


if __name__ == "__main__":
    main()
