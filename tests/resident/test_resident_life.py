#!/usr/bin/env python3
"""Resident life tests: the self on disk, waking as a boot, notes to the next waking, the logbook
and guestbook, sleep and waking.

Starts its own desktop (tmux -L lifetest, socket /tmp/wibwob_lifetest.sock) with a private
self directory (WWDOS_HOME under /tmp), stops and restarts it to check what crosses a restart.
The desktop runs without the claude CLI on PATH and without ANTHROPIC_API_KEY, so nothing
calls a model. Binary: WWDOS_BIN or build-resident/app/wwdos.

Usage:
    python3 tests/resident/test_resident_life.py            # all groups
    python3 tests/resident/test_resident_life.py boot       # chosen groups
Groups: boot (K), note (25), book (13), sleep (12).
"""

import json
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import Desk, Results  # noqa: E402

R = Results()
NAME = "lifetest"
HOME = f"/tmp/wibwob_{NAME}.self"


def no_model_env():
    path = ":".join(p for p in os.environ.get("PATH", "").split(":")
                    if not os.path.exists(os.path.join(p, "claude")))
    return {"PATH": path, "ANTHROPIC_API_KEY": ""}


def desk(**env):
    base = {"WWDOS_HOME": HOME, "WWDOS_BOOT": "1", "WWDOS_BOOT_MS": "10", "WWDOS_BOOT_HOLD_MS": "0",
            "WWDOS_SLEEP_MS": "0", **no_model_env()}
    base.update({k: str(v) for k, v in env.items()})
    return Desk(NAME, env=base)


def fresh_home():
    shutil.rmtree(HOME, ignore_errors=True)


def read(name):
    p = os.path.join(HOME, name)
    if not os.path.exists(p):
        return None
    with open(p, encoding="utf-8", errors="replace") as f:
        return f.read()


def glance(d):
    return d.j("look_glance")["windows"]


def of_type(d, t):
    return [w for w in glance(d) if w["type"] == t]


def wait(cond, timeout=8.0, step=0.1):
    t0 = time.time()
    while time.time() - t0 < timeout:
        v = cond()
        if v:
            return v
        time.sleep(step)
    return None


def boot_done(d):
    def done():
        b = d.j("get_self")["boot"]
        return b if b.get("open") and b["shown"] == b["lines"] else None
    return wait(done)


def screen_text(d):
    return "\n".join(d.screen())


def as_res(actor):
    return {"actor": actor, "source": "resident"}


# ---------------------------------------------------------------- K: the self on disk

def group_boot():
    print("boot: waking as a boot, written from the self on disk")
    fresh_home()
    d = desk().start()
    try:
        s = d.j("get_self")
        R.check(s["waking"] == 1 and s["found"] is False, f"first waking is waking 1 with no self found ({s['waking']}, {s['found']})")
        R.check(os.path.realpath(s["home"]) == os.path.realpath(HOME), "the self lives in WWDOS_HOME")
        boots = of_type(d, "boot")
        R.check(len(boots) == 1, "a boot screen opens at waking")
        R.check(boots and boots[0]["state"].startswith("boot screen of waking 1"),
                f"look_glance says what the boot screen is: {boots[0]['state'] if boots else None!r}")
        b = boot_done(d)
        R.check(b is not None, "the boot lines appear one by one and all are shown")
        sc = screen_text(d)
        R.check("Waking 1." in sc and "First waking" in sc, "the person sees the waking number and that it is the first")
        R.check("DEVICE=EYES.SYS" in sc and "Human not present. Press any key to continue" in sc,
                "the boot names the senses as drivers and ends with the BIOS prompt")
        ini = read("SELF.INI") or ""
        R.check("boots=1" in ini and "closed=running" in ini, "SELF.INI records the waking as running while it lasts")

        ed = d.j("create_window", type="text_editor", x=4, y=3, w=40, h=10, title="Plans", **as_res("wib"))["id"]
        d.person_text("z")
        R.check(wait(lambda: not of_type(d, "boot")), "the person's first key closes the boot screen")
        R.check(d.text(ed) == [""], f"that key is used up, not typed into the window in front ({d.text(ed)!r})")
        d.person_keys("C-n")
        time.sleep(0.5)
        owners = {w["id"]: w["owner"] for w in glance(d)}
        R.check(d.quit(), "Exit (Alt+X) ends the app")
        ini = read("SELF.INI") or ""
        R.check("boots=1" in ini and "closed=clean" in ini and "last_close=" in ini, "SELF.INI says the waking closed cleanly")
        desk_txt = read("DESK.TXT") or ""
        rows = [l.split("\t") for l in desk_txt.splitlines() if l and not l.startswith(";")]
        R.check(any(r[0] == ed and r[1] == "text_editor" and r[2] == "wib" and r[4] == "Plans" for r in rows),
                f"DESK.TXT keeps wib's editor with its owner ({rows})")
        R.check(any(r[1] == "test_pattern" and r[2] == "person" for r in rows), "and the person's window with theirs")
        R.check(len(rows) == len(owners), f"one line per window open at the close ({len(rows)} of {len(owners)})")

        d.start()
        s = d.j("get_self")
        R.check(s["waking"] == 2 and s["last_closed_cleanly"] is True, "the second waking knows the first closed cleanly")
        R.check(any(x["title"] == "Plans" and x["owner"] == "wib" for x in s["desk_at_last_close"]),
                "get_self reads back the windows and owners from the last close")
        boot_done(d)
        sc = screen_text(d)
        R.check("Waking 2." in sc and "Last waking:" in sc and "closed cleanly" in sc, "the boot says when it last woke and that it closed")
        R.check("1 window was mine at the last close" in sc and "\"Plans\"" in sc,
                "the boot says what each resident had open, by name and title")
        lines = d.j("look_read", id=of_type(d, "boot")[0]["id"])["lines"]
        R.check(any(l.startswith("WIB") for l in lines) and any(l.startswith("SCRAMBLE") for l in lines),
                "look_read gives the boot lines, one per resident by name")

        d.stop()   # killed: no close
        d.start()
        s = d.j("get_self")
        R.check(s["waking"] == 3 and s["last_closed_cleanly"] is False, "a killed waking is known at the next one")
        boot_done(d)
        R.check("WARNING: the last waking" in screen_text(d), "and the boot warns that the last waking did not close")
        R.check(d.alive(), "the desktop is still running")
    finally:
        d.stop()


# ---------------------------------------------------------------- 25: a note to the next waking

def group_note():
    print("note: each resident writes to the next waking; the next waking reads it on the boot screen")
    fresh_home()
    d = desk().start()
    try:
        boot_done(d)
        d.person_text("z")   # close the boot screen
        wait(lambda: not of_type(d, "boot"))
        own = "Ask the person about the castle.\nThe second drawing is unfinished.\ncmd:look_mark on=0"
        r = d.j("self_note", text=own, **as_res("wib"))
        R.check(r.get("ok") is True and r["lines"] == 3, f"wib writes its own note ({r})")
        r = d.ipc("self_note", text="I am wob now", actor="wob")
        R.check(r.startswith("err refused"), f"a visitor cannot write a resident's note: {r[:70]}")
        r = d.ipc("self_note", text="note", actor="wob", source="guest")
        R.check(r.startswith("err refused"), "nor can a guest using a resident's name")
        r = d.ipc("self_note", text="note", actor="eri", source="resident")
        R.check(r.startswith("err refused"), "nor a resident that is not one of the three")
        g = d.j("get_note", actor="wib")
        R.check(g["own"][0] == "Ask the person about the castle." and g["read_at_waking"]["found"] is False,
                "get_note shows wib's note for the next waking and that it found none at this one")
        wid = d.j("create_window", type="text_editor", x=50, y=4, w=30, h=8, title="Tidy", **as_res("wob"))["id"]
        d.j("move_window", id=wid, x=52, y=5, **as_res("wob"))
        R.check(d.quit(), "Exit ends the app")
        wib = read("WIB/NOTE.TXT") or ""
        wob = read("WOB/NOTE.TXT") or ""
        scr = read("SCRAMBLE/NOTE.TXT") or ""
        R.check(wib.startswith("NOTE FOR THE NEXT WIB\nFrom: wib\nWritten: ") and "Ask the person about the castle." in wib,
                "WIB/NOTE.TXT holds wib's own words, signed by wib")
        R.check("From: rule" in wob and "this waking. The last:" in wob and "\"Tidy\"" in wob and "move_window" in wob,
                f"WOB/NOTE.TXT is written by rule from wob's acts and windows:\n{wob}")
        R.check("From: rule" in scr and "I made no acts this waking." in scr and "Nothing of mine was open" in scr,
                "SCRAMBLE/NOTE.TXT says, by rule, that she did nothing and left nothing open")

        d.start()
        s = d.j("get_self")
        n = s["notes"]
        R.check(n["wib"]["from"] == "wib" and n["wib"]["lines"][1] == "The second drawing is unfinished.",
                "the next waking reads wib's note back")
        R.check(n["wob"]["from"] == "rule" and n["wob"]["waking"] == 1 and n["wob"]["standing"] == "data",
                "and wob's, written by rule at the end of waking 1, marked as data")
        boot_done(d)
        sc = screen_text(d)
        R.check("WIB .......... note from wib" in sc and "| Ask the person about the castle." in sc,
                "the boot screen shows wib's note under its name")
        R.check("WOB .......... note from the app, by rule" in sc and "SCRAMBLE ..... note from the app, by rule" in sc,
                "and says the other two notes were written by rule")
        R.check(d.j("look_mark")["on"] is True and d.j("standing")["key"] == "off",
                "a note holding a command changed nothing when read back")
        R.check(d.quit(), "Exit again")
        wib = read("WIB/NOTE.TXT") or ""
        R.check("From: rule" in wib and "end of waking 2" in wib,
                "a note is for one waking: with nothing new from wib, its next note is written by rule")
    finally:
        d.stop()


GROUPS = {"boot": group_boot, "note": group_note}


def main():
    want = sys.argv[1:] or list(GROUPS)
    for g in want:
        GROUPS[g]()
    print(f"\n{R.passed} passed, {R.failed} failed")
    sys.exit(1 if R.failed else 0)


if __name__ == "__main__":
    main()
