#!/usr/bin/env python3
"""Desk mosh tests: shards of the live desktop on a clock, the real desktop underneath.

Starts its own desktop (tmux -L <name>, socket /tmp/wibwob_<name>.sock) and stops it after.
Binary: WWDOS_BIN or build-resident/app/wwdos. No model is called.

Usage:
    python3 tests/resident/test_desk_mosh.py            # all groups
    python3 tests/resident/test_desk_mosh.py ways seed  # chosen groups
Groups: ways (each way changes the shown screen the way it defines), input (typing and a
drag land on the real desktop), restore (off, panic and Esc Esc give the screen back byte
for byte), seed (same seed and steps give the same frames), log (authorship, look_changes,
glance), scope (one window moshes, the rest stay clean), bench (shard counts and step times).
"""

import json
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import Desk, Results, REPO  # noqa: E402

R = Results()
PRIMER = os.path.join(REPO, "modules", "wibwob-2026", "primers", "being-wibbler.txt")
BEAT = ["kick,accent", "tick", "tick", "kick", "snare", "tick", "kick,rat", "tick",
        "kick", "tick", "snare,metal", "kick,rat"]


def cap(desk):
    r = subprocess.run(["tmux", "-L", desk.name, "capture-pane", "-p", "-N", "-e", "-t", desk.name],
                       capture_output=True, text=True)
    return r.stdout


def plain(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def m(desk, name, **kv):
    r = desk.ipc("exec_command", name=name, **kv)
    try:
        return json.loads(r)
    except json.JSONDecodeError:
        raise AssertionError(f"{name} {kv} -> {r!r}")


def steps(desk, seq):
    for h in seq:
        m(desk, "mosh_step", hits=h)
    time.sleep(0.35)


def new_desk(name, tr808=False):
    d = Desk(name, cols=160, rows=50).start()
    time.sleep(0.8)
    ed = d.editor(2, 2, 50, 14, title="notes")
    pr = d.j("create_window", type="frame_player", path=PRIMER, x=60, y=3, w=40, h=18, title="wibbler")["id"]
    if tr808:
        d.j("create_window", type="tr808", x=20, y=22, w=118, h=26, preset="electro")
    d.ipc("focus_window", id=ed)
    time.sleep(0.5)
    m(d, "mosh_tempo", bpm="0")
    return d, ed, pr


def diff_cells(a, b):
    la, lb = plain(a).split("\n"), plain(b).split("\n")
    n = 0
    for x, y in zip(la, lb):
        n += sum(1 for p, q in zip(x, y) if p != q) + abs(len(x) - len(y))
    return n


def group_ways():
    print("ways")
    d, ed, pr = new_desk("moshways")
    try:
        before = cap(d)
        w0 = d.windows()[ed]["w"]
        # wrap: real windows resized to the wrong width on kicks, shard copies read at the wrong width
        m(d, "mosh_way", way="wrap"); m(d, "mosh_on")
        steps(d, BEAT[:4])
        st = m(d, "mosh_status")
        R.check(st["showing"] == "wrap" and st["shards"] > 50, f"wrap opens shard windows ({st['shards']})")
        R.check(d.windows()[ed]["w"] != w0, f"wrap resizes the real editor on a kick ({w0} -> {d.windows()[ed]['w']})")
        R.check(diff_cells(before, cap(d)) > 500, "wrap changes the shown screen")
        m(d, "mosh_off"); time.sleep(0.3)
        R.check(d.windows()[ed]["w"] == w0, "wrap off gives the editor its width back")

        # tear: band shards shear sideways on a kick
        m(d, "mosh_way", way="tear"); m(d, "mosh_on")
        steps(d, ["tick"])
        g1 = {w["id"]: w["x"] for w in d.j("look_glance", shards="1")["windows"] if w["type"] == "mosh_shard"}
        steps(d, ["kick,accent"])
        g2 = d.j("look_glance", shards="1")["windows"]
        xs = [w["x"] for w in g2 if w["type"] == "mosh_shard"]
        moved = sum(1 for w in g2 if w["type"] == "mosh_shard" and w["id"] in g1 and g1[w["id"]] != w["x"])
        R.check(moved > 20, f"tear: a kick shears band shards sideways ({moved} moved)")
        R.check(min(xs) < 0 or max(xs) > 100, f"tear: bands leave their columns (x from {min(xs)} to {max(xs)})")
        m(d, "mosh_off"); time.sleep(0.3)

        # mosh: without refresh the shown screen smears further from the true one step by step
        m(d, "mosh_way", way="mosh"); m(d, "mosh_on")
        steps(d, ["kick"])
        d1 = diff_cells(before, cap(d))
        steps(d, ["tick"] * 8)
        d2 = diff_cells(before, cap(d))
        R.check(d2 > d1 and d2 > 300, f"mosh: blocks carry old cells along and drift ({d1} -> {d2} cells differ)")
        m(d, "mosh_off"); time.sleep(0.3)

        # bleed: colours the desktop did not have, and shade glyphs
        m(d, "mosh_way", way="bleed"); m(d, "mosh_on")
        steps(d, ["snare", "snare,metal", "snare"])
        sgr0 = set(re.findall(r"\x1b\[[0-9;]*m", before))
        on = cap(d)
        sgr1 = set(re.findall(r"\x1b\[[0-9;]*m", on))
        R.check(len(sgr1 - sgr0) >= 5, f"bleed: rotated palette shows new colours ({len(sgr1 - sgr0)} new)")
        shade = sum(plain(on).count(c) for c in "░▓█") - sum(plain(before).count(c) for c in "░▓█")
        R.check(shade > 20, f"bleed: glyphs decay to shade blocks ({shade} more)")
        m(d, "mosh_off"); time.sleep(0.3)

        # raw: a kick zooms (x2), with a framed zoomed copy of a real window
        m(d, "mosh_way", way="raw"); m(d, "mosh_on")
        steps(d, ["kick"])
        titles = [w["title"] for w in d.j("look_glance", shards="1")["windows"] if w["type"] == "mosh_shard"]
        R.check("x2" in titles, "raw: a framed x2 zoomed copy of a window opens")
        steps(d, ["snare", "snare", "rat"])
        R.check(diff_cells(before, cap(d)) > 500, "raw: scroll and a burst change the shown screen")
        m(d, "mosh_off"); time.sleep(0.3)
        R.check(cap(d) == before, "after five ways the screen is byte-identical to before")
    finally:
        d.stop()


def group_input():
    print("input")
    d, ed, pr = new_desk("moshinput")
    try:
        m(d, "mosh_way", way="tear"); m(d, "mosh_on")
        steps(d, BEAT[:3])
        d.person_type("hello")
        steps(d, ["kick"])
        d.person_type(" mosh")
        time.sleep(0.4)
        R.check(d.text(ed)[0].startswith("hello mosh"), f"typing during the mosh lands in the editor ({d.text(ed)[0]!r})")
        R.check(d.focused() == ed, "the editor keeps the focus while shards open over it")
        w = d.windows()[pr]
        x, y = w["x"] + 8, w["y"]            # the primer's title bar, under shards
        d.person_mouse("down", x, y + 1)       # screen y = desktop y + 1 (menu bar)
        d.person_mouse("up", x, y + 1)         # the first click selects, as on any Turbo Vision desktop
        time.sleep(0.8)                        # past the double-click time (a double click zooms)
        R.check(d.focused() == pr, "a click through the shards selects the real primer")
        d.person_mouse("down", x, y + 1)
        time.sleep(0.1)
        for k in range(1, 6):
            d.person_mouse("drag", x - 2 * k, y + 1 + k)
            time.sleep(0.05)
        d.person_mouse("up", x - 10, y + 6)
        time.sleep(0.4)
        w2 = d.windows()[pr]
        R.check((w2["x"], w2["y"]) == (w["x"] - 10, w["y"] + 5), f"a drag through the shards moves the real primer ({w['x']},{w['y']} -> {w2['x']},{w2['y']})")
        st = m(d, "mosh_status")
        R.check(st["on"], "the mosh keeps running through the drag")
        # a right-drag grabs a shard: it leaves the mosh and stays where it is put
        sh = [s for s in d.j("look_glance", shards="1")["windows"] if s["type"] == "mosh_shard" and s["x"] > 2 and s["y"] > 2 and s["x"] < 140]
        s0 = sh[0]
        sx, sy = s0["x"], s0["y"] + 1
        d.person_mouse("down", sx, sy, button=2)
        d.person_mouse("drag", sx + 3, sy + 2, button=2)
        d.person_mouse("up", sx + 3, sy + 2, button=2)
        time.sleep(0.3)
        R.check(m(d, "mosh_status")["held"] >= 1, "a right-drag holds a shard")
        m(d, "mosh_off"); time.sleep(0.3)
        R.check(not [s for s in d.j("look_glance", shards="1")["windows"] if s["type"] == "mosh_shard"], "off closes held shards too")
    finally:
        d.stop()


def group_restore():
    print("restore")
    d, ed, pr = new_desk("moshrestore")
    try:
        d.person_type("abc")
        time.sleep(0.3)
        before = cap(d)
        state0 = [(w["id"], w["x"], w["y"], w["w"], w["h"], w["focused"]) for w in d.state()["windows"]]
        m(d, "mosh_way", way="all"); m(d, "mosh_on")
        steps(d, BEAT * 3)
        R.check(cap(d) != before, "all ways: the screen is moshed")
        m(d, "mosh_off"); time.sleep(0.4)
        R.check(cap(d) == before, "mosh off: screen byte-identical with colours (tmux -e)")
        state1 = [(w["id"], w["x"], w["y"], w["w"], w["h"], w["focused"]) for w in d.state()["windows"]]
        R.check(state0 == state1, "mosh off: every real window's place, size, order and focus as before")
        m(d, "mosh_on"); steps(d, BEAT)
        m(d, "mosh_panic"); time.sleep(0.4)
        R.check(cap(d) == before, "mosh panic: screen byte-identical")
        m(d, "mosh_tempo", bpm="160"); m(d, "mosh_on"); time.sleep(1.0)
        R.check(m(d, "mosh_status")["step"] > 3, "the internal clock runs steps on its own")
        d.person_keys("Escape"); time.sleep(0.1); d.person_keys("Escape"); time.sleep(0.6)
        R.check(not m(d, "mosh_status")["on"], "Esc twice stops the mosh")
        R.check(cap(d) == before, "Esc Esc: screen byte-identical")
        d.person_keys("M-m"); time.sleep(0.8)
        R.check(m(d, "mosh_status")["on"], "Alt-M turns it on")
        d.person_keys("M-m"); time.sleep(0.5)
        R.check(not m(d, "mosh_status")["on"] and cap(d) == before, "Alt-M turns it off and the screen is back")
    finally:
        d.stop()


def run_seq(name, seed):
    d, ed, pr = new_desk(name)
    try:
        m(d, "mosh_seed", n=str(seed)); m(d, "mosh_way", way="all"); m(d, "mosh_on")
        frames = []
        for h in BEAT * 3:
            m(d, "mosh_step", hits=h)
            time.sleep(0.12)
            frames.append(cap(d))
        return frames
    finally:
        d.stop()


def group_seed():
    print("seed")
    a = run_seq("moshseed1", 7)
    b = run_seq("moshseed2", 7)
    c = run_seq("moshseed3", 8)
    same = sum(1 for x, y in zip(a, b) if x == y)
    R.check(same == len(a), f"same seed and steps: {same}/{len(a)} frames identical")
    other = sum(1 for x, y in zip(a, c) if x == y)
    R.check(other < len(a) // 2, f"another seed gives other frames ({other}/{len(a)} identical)")


def group_log():
    print("log")
    d, ed, pr = new_desk("moshlog")
    try:
        d.j("look_changes", reader="wib")
        n0 = len(d.j("acts", limit="500").get("acts", []))
        d.ipc("exec_command", name="mosh_on", actor="wob", source="resident")
        steps(d, BEAT)
        g = d.j("look_glance")
        R.check(all(w["type"] != "mosh_shard" for w in g["windows"]), "glance lists the real windows only by default")
        R.check(g.get("mosh", {}).get("on") is True, "glance says a mosh is showing")
        gs = d.j("look_glance", shards="1")["windows"]
        shards = [w for w in gs if w["type"] == "mosh_shard"]
        R.check(len(shards) > 50 and all(w["owner"] == "mosh" for w in shards), f"glance shards=1 lists {len(shards)} shards, owner mosh")
        ch = d.j("look_changes", reader="wib")
        R.check(ch.get("mosh", {}).get("steps") == len(BEAT) and not any(o["type"] == "mosh_shard" for o in ch["opened"]),
                f"look_changes: one mosh summary, no shard flood ({ch.get('mosh')})")
        st = d.j("look_study", x=0, y=0, w=10, h=2)
        R.check(st.get("mosh", {}).get("on") is True, "look_study says a mosh is showing")
        d.ipc("exec_command", name="mosh_off", actor="wib", source="resident")
        acts = d.j("acts", limit="500").get("acts", [])[n0:]
        texts = [(a.get("actor"), a.get("what", "")) for a in acts]
        R.check(any(a == "wob" and w.startswith("mosh on") for a, w in texts), "acts: wob turned it on")
        R.check(any(a == "wib" and w.startswith("mosh off") for a, w in texts), "acts: wib turned it off")
        R.check(not any("mosh_shard" in w for a, w in texts), f"acts: no act per shard ({len(acts)} acts)")
    finally:
        d.stop()


def group_scope():
    print("scope")
    d, ed, pr = new_desk("moshscope")
    try:
        before = plain(cap(d)).split("\n")
        m(d, "mosh_scope", scope="window", id=pr)
        m(d, "mosh_way", way="tear"); m(d, "mosh_on")
        steps(d, BEAT[:6])
        after = plain(cap(d)).split("\n")
        w = d.windows()[pr]
        inside = outside = 0
        for y, (a, b) in enumerate(zip(before, after)):
            for x, (p, q) in enumerate(zip(a, b)):
                if p == q:
                    continue
                dy = y - 1
                if w["x"] - 60 <= x < w["x"] + w["w"] + 60 and w["y"] <= dy < w["y"] + w["h"]:
                    inside += 1
                else:
                    outside += 1
        R.check(inside > 50 and outside == 0, f"scope window: only the primer's rows change ({inside} inside, {outside} outside)")
        m(d, "mosh_off")
        m(d, "mosh_scope", scope="desktop")
    finally:
        d.stop()


def group_bench():
    print("bench")
    d, ed, pr = new_desk("moshbench", tr808=True)
    try:
        d.ipc("tr808", action="play") if False else None
        for count in (100, 300, 1000):
            m(d, "mosh_count", value=str(count)); m(d, "mosh_amount", value="100")
            m(d, "mosh_tempo", bpm="128"); m(d, "mosh_way", way="tear"); m(d, "mosh_on")
            time.sleep(2.5)
            t0 = time.time(); m(d, "mosh_status"); rt = (time.time() - t0) * 1000
            st = m(d, "mosh_status")
            print(f"    count {count}: shards {st['shards']}, step {st['step_ms_avg']} ms, loop {st['loop_ms_avg']} ms, ipc {rt:.0f} ms, steps {st['step']}")
            R.check(st["shards"] > count * 0.5, f"count {count}: {st['shards']} shards open")
            m(d, "mosh_off"); time.sleep(0.3)
        m(d, "mosh_count", value="240")
    finally:
        d.stop()


def group_screen():
    print("screen")
    d, ed, pr = new_desk("moshscreen")
    try:
        before = cap(d)
        study0 = [r["text"] for r in d.j("look_study", x=0, y=0, w=160, h=20)["rows"]]
        for way in ("wrap", "tear", "mosh", "bleed", "raw"):
            m(d, "mosh_way", way=way)
            m(d, "mosh_screen", value="on")
            steps(d, BEAT[:6])
            shown = cap(d)
            R.check(diff_cells(before, shown) > 300, f"screen layer {way}: the shown screen changes ({diff_cells(before, shown)} cells)")
            st = d.j("look_study", x=0, y=0, w=160, h=20)
            R.check([r["text"] for r in st["rows"]] == study0 and st.get("mosh", {}).get("on"),
                    f"screen layer {way}: look_study reads the true frame and says a mosh is showing")
            m(d, "mosh_screen", value="off"); time.sleep(0.4)
            R.check(cap(d) == before, f"screen layer {way} off: byte-identical")
        m(d, "mosh_way", way="all"); m(d, "mosh_on"); m(d, "mosh_screen", value="on")
        steps(d, BEAT)
        d.person_type("xy"); time.sleep(0.3)
        R.check(d.text(ed)[0].startswith("xy"), "both layers on: typing lands in the editor")
        m(d, "mosh_panic"); time.sleep(0.4)
        R.check(not m(d, "mosh_status")["screen"], "panic stops the screen layer too")
        after = cap(d)
        R.check(diff_cells(before, after) <= 2, "both layers off: the screen is back (apart from the typed text)")
    finally:
        d.stop()


def colour_atoms(s):
    """The colours a capture uses, fg and bg alike: SGR codes as (kind, value)."""
    atoms = set()
    for seq in re.findall(r"\x1b\[([0-9;]*)m", s):
        p = [int(x) if x else 0 for x in seq.split(";")]
        i = 0
        while i < len(p):
            c = p[i]
            if c in (38, 48) and i + 1 < len(p) and p[i + 1] == 2 and i + 4 < len(p):
                atoms.add(("rgb", tuple(p[i + 2:i + 5]))); i += 5; continue
            if c in (38, 48) and i + 1 < len(p) and p[i + 1] == 5 and i + 2 < len(p):
                atoms.add(("x", p[i + 2])); i += 3; continue
            if 30 <= c <= 37 or 40 <= c <= 47:
                atoms.add(("x", c % 10))
            elif 90 <= c <= 97 or 100 <= c <= 107:
                atoms.add(("x", 8 + c % 10))
            i += 1
    return atoms


def shard_rects(desk):
    return [(w["x"], w["y"], w["w"], w["h"], w["z"], w["title"]) for w in desk.j("look_glance", shards="1")["windows"]
            if w["type"] == "mosh_shard"]


def covered(rects, x, y):
    return any(rx <= x < rx + rw and ry <= y < ry + rh for rx, ry, rw, rh, _, _ in rects)


def group_follow():
    print("follow")
    d, ed, pr = new_desk("moshfollow")
    try:
        m(d, "mosh_way", way="tear"); m(d, "mosh_on")
        steps(d, ["tick"])
        old = d.windows()[pr]
        d.ipc("move_window", id=pr, x=96, y=27)
        steps(d, ["tick", "tick"])
        rows = plain(cap(d)).split("\n")
        at_old = any("wibbler" in rows[y + 1][max(0, old["x"] - 8):old["x"] + old["w"] + 8] for y in range(old["y"], old["y"] + 2))
        at_new = any("wibbler" in rows[y + 1][88:150] for y in range(27, 30))
        R.check(not at_old and at_new, f"a moved window's shards take the new picture (old place {at_old}, new place {at_new})")
        d.ipc("close_window", id=ed)
        steps(d, ["tick", "tick"])
        R.check("notes" not in plain(cap(d)), "a closed window leaves nothing in the shards")
        m(d, "mosh_off"); time.sleep(0.3)
        e2 = d.editor(4, 30, 40, 10, title="scoped")
        m(d, "mosh_scope", scope="window", id=e2); m(d, "mosh_on")
        steps(d, ["kick", "tick"])
        d.ipc("close_window", id=e2)
        m(d, "mosh_step", hits="tick")
        time.sleep(0.5)
        st = m(d, "mosh_status")
        acts = [a["what"] for a in d.j("acts", limit="500")["acts"]]
        R.check(not st["on"] and st["shards"] == 0 and any("its window closed" in a for a in acts),
                "a scoped mosh's shards die with its window, and the log says why")
    finally:
        d.stop()


def group_bleed():
    print("bleed")
    d, ed, pr = new_desk("moshbleed", tr808=True)
    try:
        before = cap(d)
        a0 = colour_atoms(before)
        m(d, "mosh_way", way="bleed"); m(d, "mosh_amount", value="85"); m(d, "mosh_on")
        steps(d, ["snare", "tick", "kick", "snare,metal", "tick", "snare"])
        on = cap(d)
        a1 = colour_atoms(on)
        R.check(a1 <= a0, f"bleed keeps to the desktop's own palette ({len(a1 - a0)} foreign colours)")
        shade = sum(plain(on).count(c) for c in "░▓█") - sum(plain(before).count(c) for c in "░▓█")
        R.check(shade > 50, f"bleed decays glyphs to shade blocks ({shade})")
        blank = plain(on).count(" ") - plain(before).count(" ")
        R.check(blank < 300, f"decay leaves texture, not blank areas ({blank} more blank cells)")
        R.check(d.windows()[ed] and m(d, "mosh_status")["on"], "real windows keep their own colours (no window recolouring)")
        m(d, "mosh_off")
    finally:
        d.stop()


def group_hold():
    print("hold")
    d, ed, pr = new_desk("moshhold")
    try:
        d.person_type("abc")
        m(d, "mosh_way", way="tear"); m(d, "mosh_on")
        steps(d, ["tick", "tick"])
        rects = shard_rects(d)
        front = sorted([r for r in rects if r[0] > 4 and r[1] > 20 and r[0] + r[2] < 150 and r[1] < 40 and r[2] >= 3], key=lambda r: r[4])
        sx, sy = front[0][0] + 1, front[0][1]
        d.person_mouse("down", sx, sy + 1)
        time.sleep(0.7)
        for k in range(1, 4):
            d.person_mouse("drag", sx + 2 * k, sy + 1 + k)
            time.sleep(0.2)
        time.sleep(0.3)
        d.person_mouse("up", sx + 6, sy + 4)
        time.sleep(0.4)
        st = m(d, "mosh_status")
        moved = [r for r in shard_rects(d) if (r[0], r[1]) == (front[0][0] + 6, front[0][1] + 3)]
        R.check(st["held"] >= 1 and moved, f"a left press held still grabs a frameless shard and drags it (held {st['held']})")
        d.ipc("focus_window", id=ed)
        steps(d, ["tick"])
        cur = subprocess.run(["tmux", "-L", d.name, "display", "-p", "-t", d.name, "#{cursor_x},#{cursor_y},#{cursor_flag}"],
                             capture_output=True, text=True).stdout.strip().split(",")
        cx, cy, flag = int(cur[0]), int(cur[1]) - 1, cur[2]
        caret = m(d, "mosh_status")["caret"]
        steps(d, ["tick"])
        rects = shard_rects(d)
        cx, cy = caret
        R.check(caret == [6, 3] and not covered(rects, cx, cy), f"the caret (after abc, at {caret}) is not covered")
        R.check(all(covered(rects, x, y) for x, y in ((cx - 1, cy), (cx + 1, cy), (cx, cy - 1), (cx, cy + 1))),
                "the hole around the caret is one cell: its four neighbours are shards")
        m(d, "mosh_off")
    finally:
        d.stop()


def group_sound():
    print("sound")
    d = Desk("moshsound", cols=160, rows=50, env={"WWDOS_SOUND_OUT": "null", "WWDOS_SYNTH_AUDIO": "0"}).start()
    try:
        time.sleep(0.8)
        d.editor(2, 2, 50, 14, title="notes")
        tid = d.j("create_window", type="tr808", x=20, y=20, w=118, h=27)["id"]
        d.ipc("exec_command", name="tr808", action="clear", all="1")
        d.ipc("exec_command", name="tr808", action="toggle_step", instrument="bd", step="0")
        d.ipc("exec_command", name="tr808", action="tempo", bpm="120")
        m(d, "mosh_tempo", bpm="0"); m(d, "mosh_way", way="mosh"); m(d, "mosh_on")
        d.ipc("exec_command", name="tr808", action="play")
        time.sleep(2.2)
        st = m(d, "mosh_status")
        d.ipc("exec_command", name="tr808", action="stop")
        R.check(st["step"] >= 12 and st["clock"] == "tr808", f"silent drum steps move the mosh: {st['step']} steps from 2 hits a bar")
        m(d, "mosh_off"); time.sleep(0.3)
        m(d, "mosh_way", way="sound"); m(d, "mosh_on")
        d.ipc("exec_command", name="synth", action="notes", notes="C-2", step="200")
        time.sleep(0.5)
        s1 = m(d, "mosh_status")
        d.ipc("exec_command", name="synth", action="notes", notes="E-5 G-5", step="120")
        time.sleep(0.5)
        s2 = m(d, "mosh_status")
        R.check(s1["showing"] == "wrap" and s1["step"] >= 1, f"a bass note wraps ({s1['showing']}, {s1['step']} steps)")
        R.check(s2["showing"] == "tear" and s2["step"] > s1["step"], f"note-ons tear ({s2['showing']})")
        m(d, "mosh_off"); time.sleep(0.3)
        m(d, "mosh_way", way="sound"); m(d, "mosh_on")
        d.ipc("exec_command", name="open_tracker", track="arp-and-bass", play="1", x="2", y="2", w="80", h="18")
        seen = set()
        for _ in range(10):
            time.sleep(0.2)
            seen.add(m(d, "mosh_status")["showing"])
        st = m(d, "mosh_status")
        d.ipc("exec_command", name="tracker", action="stop")
        R.check(st["step"] >= 15 and seen & {"wrap", "tear"}, f"the tracker's notes and drum rows move it too ({st['step']} steps, ways {sorted(seen)})")
        m(d, "mosh_off")
    finally:
        d.stop()


def group_residents():
    print("residents")
    from test_resident_world import FAKE_CLAUDE
    fake = "/tmp/wibwob_moshres_fake"
    os.makedirs(fake, exist_ok=True)
    with open(f"{fake}/claude", "w") as f:
        f.write(FAKE_CLAUDE)
    os.chmod(f"{fake}/claude", 0o755)
    with open(f"{fake}/reply.txt", "w") as f:
        f.write("nothing yet\n")
    env = {"PATH": f"{fake}:/usr/bin:/bin:/usr/sbin:/sbin", "ANTHROPIC_API_KEY": "", "FAKE_DIR": fake}
    d = Desk("moshres", cols=160, rows=50, env=env).start()
    try:
        time.sleep(1.0)
        d.editor(2, 2, 50, 14, title="notes")
        m(d, "mosh_tempo", bpm="0")
        d.ipc("open_scramble", actor="setup")
        time.sleep(0.5)
        with open(f"{fake}/reply.txt", "w") as f:
            f.write('there. the windows are coming apart. (=^..^=)\nACT: mosh args="way tear"\nACT: mosh args="on"\n')
        d.ipc("scramble_say", text="make the desk mosh", actor="setup")
        t0 = time.time()
        while time.time() - t0 < 8 and not m(d, "mosh_status")["on"]:
            time.sleep(0.2)
        st = m(d, "mosh_status")
        R.check(st["on"] and st["way"] == "tear" and st["by"] == "scramble", f"Scramble's paws start and steer the mosh ({st['by']}, {st['way']})")
        args = open(f"{fake}/args.txt").read() if os.path.exists(f"{fake}/args.txt") else ""
        R.check("mosh args=" in args, "her prompt names the mosh among her paws")
        steps(d, ["kick", "tick"])
        with open(f"{fake}/reply.txt", "w") as f:
            f.write('stopped. (=^..^=)\nACT: mosh_off\n')
        time.sleep(2.5)
        d.ipc("scramble_say", text="stop it", actor="setup")
        t0 = time.time()
        while time.time() - t0 < 8 and m(d, "mosh_status")["on"]:
            time.sleep(0.2)
        R.check(not m(d, "mosh_status")["on"], "Scramble stops the mosh")
        node = subprocess.run(["node", os.path.join(REPO, "app/llm/sdk_bridge/app_socket.js"), "wob", "mosh_on", "way=mosh"],
                              capture_output=True, text=True, env={**os.environ, "WWDOS_SOCKET": d.sock})
        st = m(d, "mosh_status")
        R.check(node.returncode == 0 and st["on"] and st["by"] == "wob", f"the chat's tool starts it as wob ({node.stdout.strip()[:40]!r})")
        caps = d.ipc("get_capabilities")
        R.check("mosh_on" in caps and "mosh_score" in caps, "the mosh commands are in the list the chat's tool reads")
        steps(d, ["kick", "tick"])
        rects = sorted([r for r in shard_rects(d) if 30 < r[0] < 90 and 10 < r[1] < 30], key=lambda r: r[4])
        sx, sy = rects[0][0], rects[0][1]
        d.ipc("resident_mouse", action="drag", button="right", x=sx, y=sy, x2=sx + 40, y2=sy + 6, steps="3",
              actor="wib", source="resident")
        time.sleep(2.5)
        st = m(d, "mosh_status")
        acts = [(a["actor"], a["what"]) for a in d.j("acts", limit="800")["acts"]]
        R.check(st["held"] >= 1 and any(a == "wib" and ("threw" in w or "grabbed" in w) for a, w in acts),
                f"a resident's hand grabs a shard ({[w for a, w in acts if 'shard' in w][-1:]})")
        R.check(any(a == "wib" and "threw" in w for a, w in acts), "and throws it")
        m(d, "mosh_off")
    finally:
        d.stop()


def group_score():
    print("score")
    d, ed, pr = new_desk("moshscore")
    try:
        r = m(d, "mosh_score", path="scores/catmosh.mosh")
        R.check(r["score_bars"] == 40, f"the catmosh score loads: 5 parts, {r['score_bars']} bars")
        m(d, "mosh_on")
        seen = []
        for bar in range(40):
            seen.append(m(d, "mosh_step", hits="tick", n="16")["showing"])
        want = ["wrap"] * 5 + ["tear"] * 9 + ["mosh"] * 7 + ["bleed"] * 12 + ["raw"] * 7
        R.check(seen == want, f"the mosh follows the score bar by bar in the film's order ({seen[::4]})")
        m(d, "mosh_step", hits="tick")
        time.sleep(0.5)
        R.check(not m(d, "mosh_status")["on"], "at the score's end the mosh stops")
        bad = d.ipc("exec_command", name="mosh_score", text="bars 2 wobble")
        R.check(bad.startswith("err"), f"a bad score is refused with its line ({bad})")
    finally:
        d.stop()


def group_timelag():
    print("timelag")
    d, ed, pr = new_desk("moshtime")
    try:
        m(d, "mosh_way", way="timelag"); m(d, "mosh_tempo", bpm="0"); m(d, "mosh_on")
        letters = "abcdefghijklmnopqrstuvwxyz0123"
        for k in range(30):
            d.person_text(letters[k])
            time.sleep(0.06)
            m(d, "mosh_step", hits="tick")
        time.sleep(0.3)
        now = d.text(ed)[0]
        shown = plain(cap(d)).split("\n")[1 + 3][3:3 + 40].rstrip()   # the editor's first line on screen
        st = m(d, "mosh_status")
        R.check(now == letters and shown and shown != now and now.startswith(shown),
                f"timelag shows the editor as it was steps ago: {shown!r} of {now!r} (map {st['lag_map']}, lag {st['lag']} s)")
        for _ in range(40):
            m(d, "mosh_step", hits="tick")
        st = m(d, "mosh_status")
        R.check(st["ring_frames"] <= 48 and st["ring_bytes"] <= 48 * 160 * 48 * 32, f"the ring of past frames is bounded ({st['ring_frames']} frames, {st['ring_bytes']} bytes)")
        l0 = st["lag"]
        for _ in range(4):
            m(d, "mosh_step", hits="kick,metal")
        l1 = m(d, "mosh_status")["lag"]
        R.check(l1 > l0, f"drum hits widen the lag ({l0} -> {l1} s)")
        d.ipc("exec_command", name="synth", action="notes", notes="C-4", step="3000")
        time.sleep(0.3)
        for _ in range(4):
            m(d, "mosh_step", hits="tick")
        l2 = m(d, "mosh_status")["lag"]
        R.check(l2 < l1 * 0.6, f"a held synth note narrows it ({l1} -> {l2} s)")
        m(d, "mosh_off"); time.sleep(0.3)
    finally:
        d.stop()


def group_live():
    print("live")
    d = Desk("moshlive", cols=160, rows=50).start()
    try:
        time.sleep(0.8)
        d.ipc("exec_command", name="open_terminal", x="4", y="3", w="60", h="16", title="film",
              exec="sh -c 'i=0; while true; do i=$((i+1)); printf \"\\033[2J\\033[HFRAME %d\\n\" $i; sleep 0.25; done'")
        time.sleep(1.2)
        m(d, "mosh_tempo", bpm="0"); m(d, "mosh_way", way="tear"); m(d, "mosh_amount", value="20"); m(d, "mosh_on")
        steps(d, ["tick"])
        a = re.findall(r"FRAME (\d+)", plain(cap(d)))
        time.sleep(1.2)                        # no steps: the film plays on in the shards
        b = re.findall(r"FRAME (\d+)", plain(cap(d)))
        st = m(d, "mosh_status")
        R.check(a and b and max(map(int, b)) > max(map(int, a)) and st["step"] == 1,
                f"a film in a terminal window moshes as a live source: frames {a[:1]} -> {b[:1]} with no new step")
        m(d, "mosh_off")
    finally:
        d.stop()


def group_keys():
    print("keys")
    d = Desk("moshkeys", cols=160, rows=50, env={"WWDOS_MIDI": "null", "WWDOS_SOUND_OUT": "null", "WWDOS_SYNTH_AUDIO": "0"}).start()
    try:
        time.sleep(0.8)
        ed = d.editor(2, 2, 50, 14, title="notes")
        m(d, "mosh_tempo", bpm="0"); m(d, "mosh_way", way="tear"); m(d, "mosh_on")
        s0 = m(d, "mosh_status")["step"]
        for k in ("S-F1", "S-F2", "S-F4", "S-F5", "S-F6"):
            d.person_keys(k); time.sleep(0.2)
        st = m(d, "mosh_status")
        R.check(st["step"] == s0 + 5, f"Shift+F1, F2, F4, F5, F6 play hits by hand ({s0} -> {st['step']} steps)")
        R.check(d.text(ed)[0] == "", "the keys are used up, not typed")
        d.person_keys("S-F7"); time.sleep(0.3)
        R.check(m(d, "mosh_status")["way"] == "mosh", f"Shift+F7 changes way ({m(d, 'mosh_status')['way']})")
        d.person_keys("S-F8"); time.sleep(0.3)
        st = m(d, "mosh_status")
        d.person_keys("S-F1"); time.sleep(0.3)
        R.check(st["frozen"] and m(d, "mosh_status")["step"] == st["step"], "Shift+F8 freezes: hits take no step")
        d.person_keys("S-F8"); time.sleep(0.3)
        R.check(not m(d, "mosh_status")["frozen"], "Shift+F8 again releases")
        d.person_keys("S-F9"); time.sleep(0.2)
        d.person_keys("S-F10"); time.sleep(0.2)
        d.person_keys("S-F9"); time.sleep(0.2)
        R.check(m(d, "mosh_status")["amount"] == 60, "Shift+F9 and Shift+F10 take the amount down and up by 10")
        s1 = m(d, "mosh_status")["step"]
        d.ipc("exec_command", name="midi", action="in", data="99 24 70")
        d.ipc("exec_command", name="midi", action="in", data="99 26 70")
        time.sleep(0.3)
        R.check(m(d, "mosh_status")["step"] == s1 + 2, "a MIDI keyboard's drum notes play hits")
        d.ipc("exec_command", name="midi", action="in", data="B0 01 20")
        time.sleep(0.2)
        R.check(m(d, "mosh_status")["amount"] == 25, f"the mod wheel (CC 1) sets the amount ({m(d, 'mosh_status')['amount']})")
        d.ipc("exec_command", name="midi", action="in", data="B0 40 7F"); time.sleep(0.2)
        fz = m(d, "mosh_status")["frozen"]
        d.ipc("exec_command", name="midi", action="in", data="B0 40 00"); time.sleep(0.2)
        R.check(fz and not m(d, "mosh_status")["frozen"], "the sustain pedal (CC 64) freezes while held")
        m(d, "mosh_off")
        d.ipc("exec_command", name="midi", action="in", data="90 3C 64"); time.sleep(0.3)
        R.check(m(d, "mosh_status")["step"] == 0 or not m(d, "mosh_status")["on"], "with the mosh off MIDI notes go to the synth as before")
    finally:
        d.stop()


def group_record():
    print("record")
    rec, cast = "/tmp/wibwob_moshrec.mosh", "/tmp/wibwob_moshrec.cast"
    d, ed, pr = new_desk("moshrec")
    try:
        before = cap(d)
        m(d, "mosh_seed", n="5"); m(d, "mosh_way", way="all")
        r = m(d, "mosh_record", value="on", path=rec, cast=cast)
        R.check(r["recording"] and r["on"], "recording starts a fresh mosh")
        for h in BEAT * 2:
            m(d, "mosh_step", hits=h)
        m(d, "mosh_way", way="timelag")
        d.person_keys("S-F7"); time.sleep(0.2)
        m(d, "mosh_hit", hits="kick,metal")
        for h in BEAT:
            m(d, "mosh_step", hits=h)
        time.sleep(0.3)
        a = cap(d)
        m(d, "mosh_record", value="off")
        m(d, "mosh_off"); time.sleep(0.3)
        lines = open(rec).read().splitlines()
        R.check(lines[1] == "seed 5" and sum(1 for l in lines if " step " in l) == len(BEAT) * 3 + 1,
                f"the recording holds the seed and every step ({len(lines)} lines)")
        m(d, "mosh_seed", n="99"); m(d, "mosh_way", way="tear")
        p = m(d, "mosh_play", path=rec, fast="1")
        time.sleep(0.5)
        b = cap(d)
        R.check(a == b, "playing it back gives the same frame, byte for byte")
        st = m(d, "mosh_status")
        R.check(st["seed"] == 5 and not st["playing"], "playback takes the recording's seed and ends")
        cl = open(cast).read().splitlines()
        hdr = json.loads(cl[0])
        evs = [json.loads(x) for x in cl[1:]]
        R.check(hdr["version"] == 2 and hdr["width"] == 160 and hdr["height"] == 50 and len(evs) >= len(BEAT) * 2,
                f"the cast is asciicast v2, 160x50, {len(evs)} frames")
        R.check(all(e[1] == "o" for e in evs) and evs == sorted(evs, key=lambda e: e[0]) and "notes" in "".join(e[2] for e in evs),
                "its frames are output events in time order, with the desktop's text")
        m(d, "mosh_off"); time.sleep(0.3)
        R.check(cap(d) == before, "the desktop comes back after playback")
    finally:
        d.stop()


GROUPS = {"timelag": group_timelag, "live": group_live, "keys": group_keys, "record": group_record,
          "follow": group_follow, "bleed": group_bleed, "hold": group_hold, "sound": group_sound,
          "residents": group_residents, "score": group_score, "screen": group_screen, "ways": group_ways, "input": group_input, "restore": group_restore, "seed": group_seed,
          "log": group_log, "scope": group_scope, "bench": group_bench}

if __name__ == "__main__":
    chosen = sys.argv[1:] or list(GROUPS)
    for g in chosen:
        try:
            GROUPS[g]()
        except AssertionError as e:
            R.check(False, f"{g}: {e}")
    print(f"\n{R.passed} passed, {R.failed} failed")
    sys.exit(1 if R.failed else 0)
