#!/usr/bin/env python3
"""Resident senses tests: looking without touching, authorship, who is speaking.

Starts its own desktop (tmux -L <name>, socket /tmp/wibwob_<name>.sock) and stops it after.
Binary: WWDOS_BIN or build-resident/app/wwdos.

Usage:
    python3 tests/resident/test_resident_senses.py            # all groups
    python3 tests/resident/test_resident_senses.py look       # chosen groups
Groups: look (glance, read, study, changes, mark).
"""

import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import Desk, Results  # noqa: E402

R = Results()


def q(desk, cmd, **kv):
    """Queue a resident command and wait for every item it queued."""
    r = desk.ipc(cmd, **kv)
    if not r.startswith("{"):
        return r, {}
    j = json.loads(r)
    if not j.get("ok"):
        return r, {}
    return j, desk.wait_seqs(j["first"], j["last"])


def screen_with_colours(desk):
    """Every cell with its colours, as tmux shows them (-e keeps the escape sequences, -N trailing cells)."""
    r = subprocess.run(["tmux", "-L", desk.name, "capture-pane", "-p", "-N", "-e", "-t", desk.name],
                       capture_output=True, text=True)
    return r.stdout


def layout(desk):
    """Focus, z-order and rectangles: what a look must not change."""
    return [(w["id"], w["x"], w["y"], w["w"], w["h"], w["z"], w["focused"], w["title"]) for w in desk.state()["windows"]]


def settle(desk, t=0.4):
    time.sleep(t)


def marks_off(desk):
    return desk.j("look_mark", on="0")


def marks_on(desk, ms=1500):
    return desk.j("look_mark", on="1", ms=str(ms))


# ---------------------------------------------------------------------------- look

def test_look(desk):
    print("\n[look] glance, read, study, changes, mark")
    desk.ipc("close_all")
    settle(desk, 0.3)
    a = desk.editor(2, 1, 40, 8, "Alpha")
    b = desk.editor(60, 1, 40, 12, "Beta")
    hidden = desk.editor(64, 3, 20, 6, "Under")        # opened before Cover: Cover sits in front of it
    cover = desk.j("create_window", type="gradient", x=62, y=2, w=30, h=10, title="Cover")["id"]
    pattern = desk.j("create_window", type="test_pattern", x=4, y=12, w=30, h=10, title="Pattern")["id"]
    lines = [f"line {i:02d} of the alpha text" for i in range(20)]
    q(desk, "resident_key", id=a, text="\n".join(lines))
    q(desk, "resident_key", id=hidden, text="under the cover")
    settle(desk)

    # ---- nothing changes: marks off, every look command, screen with colours before and after
    R.check(marks_off(desk)["on"] is False, "marks can be switched off")
    settle(desk, 0.2)
    before_screen = screen_with_colours(desk)
    before_layout = layout(desk)
    before_text = {w: desk.ipc("window_text", id=w) for w in (a, b, hidden)}
    g = desk.j("look_glance", reader="wib")
    reads = {w: desk.j("look_read", id=w, reader="wib") for w in (a, b, hidden, cover, pattern)}
    st_screen = desk.j("look_study", x=0, y=0, w=100, h=24, reader="wib")
    st_win = desk.j("look_study", id=pattern, x=0, y=0, w=30, h=10, reader="wib")
    ch = desk.j("look_changes", reader="wib")
    for _ in range(20):
        desk.ipc("look_glance")
        desk.ipc("look_read", id=a)
    settle(desk, 0.3)
    after_screen = screen_with_colours(desk)
    R.check(before_screen == after_screen,
            f"the screen, colours included, is byte-identical after {7 + 40 + 3} look commands ({len(before_screen)} bytes)")
    R.check(layout(desk) == before_layout, "focus, z-order, rectangles and titles are unchanged")
    R.check(all(desk.ipc("window_text", id=w) == before_text[w] for w in before_text), "editor contents are unchanged")

    # ---- glance
    wins = {w["id"]: w for w in g["windows"]}
    R.check(set(wins) >= {a, b, hidden, cover, pattern}, f"glance lists every window ({len(wins)})")
    keys = {"id", "type", "title", "x", "y", "w", "h", "z", "owner", "focused", "seen", "state"}
    R.check(all(keys <= set(w) for w in wins.values()), "each line carries id, type, title, rect, z, owner, seen, state")
    R.check(wins[a]["type"] == "text_editor" and wins[pattern]["type"] == "test_pattern", "types come from the registry")
    R.check(wins[a]["x"] == 2 and wins[a]["y"] == 1 and wins[a]["w"] == 40 and wins[a]["h"] == 8, "rects are desktop cells")
    R.check(wins[hidden]["seen"] == "none", f"a window behind another is reported unseen: {wins[hidden]['seen']}")
    R.check(wins[b]["seen"] == "part", f"a partly covered window is reported part-seen: {wins[b]['seen']} {wins[b].get('seen_pct')}%")
    R.check(wins[a]["seen"] == "all", "an uncovered window is reported seen")
    R.check(wins[pattern]["focused"] and g["focus"] == pattern, "focus is reported")
    R.check("20 lines" in wins[a]["state"], f"the editor's sentence counts its lines: {wins[a]['state']!r}")
    R.check(wins[pattern]["state"] not in ("", "blank"), f"a window without a model gets a sentence from its cells: {wins[pattern]['state']!r}")
    R.check(g.get("changes", {}).get("first") is True, "the reader's first glance starts its record of what it saw")

    # ---- read
    ra = reads[a]
    R.check(ra["from"] == "model" and ra["lines"] == lines,
            f"read gives the editor's whole text from its model, scrolled-off lines included ({len(ra['lines'])} lines in a window 6 rows high)")
    R.check(reads[hidden]["lines"] == ["under the cover"], "read gives the text of a window the person cannot see")
    rp = reads[pattern]
    R.check(rp["from"] == "cells" and len(rp["lines"]) > 0 and any(l.strip() for l in rp["lines"]),
            f"a window without a model is read from its cells ({len(rp['lines'])} rows)")
    R.check(reads[cover]["from"] == "cells", "the gradient is read from its cells")

    # ---- study
    rows = desk.screen()
    R.check(st_screen["ruler"][1][:12] == "012345678901" and st_screen["ruler"][0][:11] == "0         1",
            "the ruler numbers the columns (tens and units)")
    ok_rows = 0
    for r in st_screen["rows"]:
        sy = r["y"] + 1   # desktop y 0 is screen row 1
        if rows[sy][:100].rstrip() == r["text"].rstrip():
            ok_rows += 1
    R.check(ok_rows == 24, f"study's text matches the terminal's rows ({ok_rows}/24)")
    R.check(all(sum(run[1] for run in r["runs"]) == 100 for r in st_screen["rows"]),
            "colour runs cover every cell of every row")
    R.check(all(0 <= run[2] <= 15 and 0 <= run[3] <= 15 for r in st_screen["rows"] for run in r["runs"]),
            "colours are BIOS numbers 0-15")
    R.check(st_win["from"] == "window" and len(st_win["rows"]) == 10, "study of a window reads its own picture")
    bad = desk.ipc("look_study", x=0, y=0, w=500, h=5)
    R.check(bad.startswith("err rectangle outside"), "a rectangle off the screen is refused")

    # ---- changes since the reader last looked
    R.check(ch["first"] is False and ch["opened"] == [] and ch["closed"] == [] and ch["moved"] == [],
            "nothing opened, closed or moved between two looks")
    desk.j("move_window", id=pattern, x=6, y=13)
    q(desk, "resident_key", id=a, text="!")
    desk.j("close_window", id=hidden)
    new = desk.editor(110, 14, 30, 8, "Gamma")
    desk.ipc("exec_command", name="window_title", id=cover, title="Cover2")
    settle(desk, 0.3)
    ch = desk.j("look_changes", reader="wib")
    R.check([o["id"] for o in ch["opened"]] == [new], f"changes: opened {ch['opened']}")
    R.check([c["id"] for c in ch["closed"]] == [hidden], f"changes: closed {ch['closed']}")
    R.check([m["id"] for m in ch["moved"]] == [pattern] and ch["moved"][0]["to"][:2] == [6, 13], f"changes: moved {ch['moved']}")
    R.check([t["id"] for t in ch["retitled"]] == [cover] and ch["retitled"][0]["to"] == "Cover2", f"changes: retitled {ch['retitled']}")
    R.check(a in ch["content"], f"changes: the edited window's content changed {ch['content']}")
    R.check(ch.get("focus", {}).get("to") == new, f"changes: focus moved to the new window {ch.get('focus')}")
    other = desk.j("look_changes", reader="wob")
    R.check(other["first"] is True, "another reader has its own record")
    ch2 = desk.j("look_changes", reader="wib")
    R.check(ch2["opened"] == [] and ch2["closed"] == [] and ch2["moved"] == [], "asking again: nothing new")

    # ---- the reading mark
    desk.j("focus_window", id=b)
    settle(desk, 0.3)
    base = desk.screen()
    base_layout = layout(desk)
    marks_on(desk, ms=1500)
    desk.j("look_read", id=a, reader="wib")
    settle(desk, 0.3)
    marked = desk.screen()
    diff = [(y, x) for y in range(min(len(base), len(marked))) for x in range(max(len(base[y]), len(marked[y])))
            if (base[y][x:x + 1] or " ") != (marked[y][x:x + 1] or " ")]
    wa = desk.windows()[a]
    R.check(len(diff) == 1, f"the mark changes one cell on screen ({len(diff)}: {diff[:4]})")
    if diff:
        y, x = diff[0]
        R.check(y == wa["y"] + 1 and wa["x"] <= x < wa["x"] + wa["w"],
                f"the mark sits on the read window's top frame row (screen {x},{y})")
        R.check(marked[y][x] == "w", f"the mark is the reader's initial: {marked[y][x]!r}")
    m = desk.j("look_mark")
    R.check(m["on"] and [k["window"] for k in m["marks"]] == [a], f"look_mark reports where wib reads: {m['marks']}")
    R.check(layout(desk) == base_layout, "the mark does not change focus, z-order or rectangles")
    desk.j("look_read", id=b, reader="wib")
    settle(desk, 0.3)
    m = desk.j("look_mark")
    R.check([k["window"] for k in m["marks"]] == [b], "the mark follows the reader to the next window")
    time.sleep(1.8)
    gone = desk.screen()
    R.check(gone == base and desk.j("look_mark")["marks"] == [], "the mark is gone after its time and the screen is as before")
    marks_off(desk)
    desk.j("look_read", id=a, reader="wib")
    settle(desk, 0.3)
    R.check(desk.screen() == base, "with marks off a read leaves no mark")


def main():
    groups = [g.lower() for g in sys.argv[1:]] or ["look"]
    desk = Desk(os.environ.get("SENSES_TEST_NAME", "sensestest")).start()
    try:
        for g in groups:
            {"look": test_look}[g](desk)
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
