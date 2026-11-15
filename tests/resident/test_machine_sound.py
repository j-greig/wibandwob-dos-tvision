#!/usr/bin/env python3
"""The machine's sound: the synth, the ears and event scores, on a desktop of its own.

Starts its own desktop (tmux -L soundtest, socket /tmp/wibwob_soundtest.sock) with the synth's
sound off (WWDOS_SYNTH_AUDIO=0) and the TR-808's off, without the claude CLI on PATH, and stops
it after. Binary: WWDOS_BIN or build-resident/app/wwdos.

Usage:
    python3 tests/resident/test_machine_sound.py            # all groups
    python3 tests/resident/test_machine_sound.py synth      # chosen groups
Groups: synth, ears.
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


# ---------------------------------------------------------------------------- ears

def test_ears(desk):
    print("\n[ears] what sounds now and what sounded, for the synth and the TR-808")
    desk.ipc("close_all")
    time.sleep(0.3)
    syn = x(desk, "open_synth", x=2, y=1, w=60, h=18)["id"]
    drm = x(desk, "open_tr808", x=10, y=20, w=118, h=28)["id"]
    time.sleep(2.2)   # the ears outlive closed windows: let the last group's notes age past 2 s

    n0 = last_act(desk)
    h = x(desk, "hear", seconds=2, actor="wib", source="resident")
    R.check(h["standing"] == "data" and h["counts"] == {"synth": 0, "tr808": 0} and "silence" in h["summary"],
            f"before anything plays the ears hear silence ({h['summary']!r})")

    # wib plays a phrase; the ears hear it at once, the first note sounding.
    x(desk, "synth", id=syn, action="notes", notes="C-4 E-4 G-4", step=200, dur=150, actor="wib", source="resident")
    h = x(desk, "hear", seconds=2, of="synth", actor="wob")
    R.check(any(s["note"] == "C-4" and s["who"] == "wib" for s in h["sounding"]),
            f"right after the phrase starts, C-4 by wib is sounding ({h['sounding']})")
    time.sleep(0.9)
    h = x(desk, "hear", seconds=2, of="synth", actor="wob")
    R.check([e["note"] for e in h["heard"]] == ["C-4", "E-4", "G-4"] and all(e["who"] == "wib" for e in h["heard"]),
            f"wob hears wib's phrase: C-4 E-4 G-4 ({[e['note'] for e in h['heard']]})")
    gaps = [h["heard"][i]["ago_ms"] - h["heard"][i + 1]["ago_ms"] for i in range(2)]
    R.check(all(abs(g - 200) <= 2 for g in gaps), f"the notes are heard 200 ms apart, as played ({gaps})")
    R.check(h["sounding"] == [] and "Nothing sounds now" in h["summary"], "after the phrase nothing sounds")
    R.check("synth 3 notes (C-4 to G-4) by wib 3" in h["summary"], f"summary: {h['summary']!r}")
    R.check(any("C-4" in r for r in h["rows"]) and len(h["rows"]) == 16, f"tracker rows of 125 ms cover the 2 s ({len(h['rows'])} rows)")

    # wob starts the drum machine; the ears hear its steps as the engine plays them.
    st = desk.j("exec_command", name="tr808", id=drm, action="state")
    preset = st["presets"][0]
    x(desk, "tr808", id=drm, action="load_preset", preset=preset)
    x(desk, "tr808", id=drm, action="play", actor="wob", source="resident")
    time.sleep(1.3)
    h = x(desk, "hear", seconds=1, of="tr808")
    st = x(desk, "tr808", id=drm, action="state")
    pat = {i["id"]: i["steps"] for i in st["instruments"]}
    hits = h["heard"]
    R.check(len(hits) > 0 and h["counts"]["synth"] == 0, f"the ears hear {len(hits)} TR-808 hits in the last second and no synth")
    R.check(all(pat[e["voice"]][e["step"]] == "x" for e in hits),
            "every hit heard is a step the pattern has on (voice and step match the engine's pattern)")
    R.check(all(e["who"] == "wob" and e["tempo"] == 120 for e in hits), "each hit names wob, who started it, and 120 bpm")
    steps = sorted({e["step"] for e in hits})
    R.check(len(steps) >= 4, f"the hits come from at least four steps ({steps})")
    R.check("tr808" in h["summary"] and "started by wob" in h["summary"], f"summary: {h['summary']!r}")
    hw = x(desk, "hear", seconds=1, window=syn)
    R.check(hw["counts"]["tr808"] == 0, "hear window= keeps one window's sounds")
    g = glance_of(desk, drm) or {}
    R.check(g.get("state", "").startswith(f"TR-808 drum machine, playing pattern A1-A at 120 bpm, step"),
            f"look_glance gives the drum machine one sentence: {g.get('state')!r}")
    x(desk, "tr808", id=drm, action="stop")

    # Hearing changes nothing and logs nothing.
    acts = [a for a in acts_since(desk, n0) if "hear" in a["what"]]
    R.check(acts == [], "hear is not logged as an act (it changes nothing)")
    R.check(any(line.find(" ears ") >= 0 for line in desk.screen()), "the synth window shows the ears' meter to the person")


GROUPS = {"synth": test_synth, "ears": test_ears}


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
