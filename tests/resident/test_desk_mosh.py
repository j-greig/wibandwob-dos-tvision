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


GROUPS = {"screen": group_screen, "ways": group_ways, "input": group_input, "restore": group_restore, "seed": group_seed,
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
