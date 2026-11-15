#!/usr/bin/env python3
"""The machine's sound: the synth, the ears and event scores, on a desktop of its own.

Starts its own desktop (tmux -L soundtest, socket /tmp/wibwob_soundtest.sock) with the synth's
sound off (WWDOS_SYNTH_AUDIO=0) and the TR-808's off, without the claude CLI on PATH, and stops
it after. Binary: WWDOS_BIN or build-resident/app/wwdos.

Usage:
    python3 tests/resident/test_machine_sound.py            # all groups
    python3 tests/resident/test_machine_sound.py synth      # chosen groups
Groups: synth, ears, score, hands (its own desktop with a stand-in claude CLI), stream.
"""

import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from resident_lib import REPO, Desk, Results  # noqa: E402

R = Results()
QUIET = {"PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "ANTHROPIC_API_KEY": "", "WWDOS_SYNTH_AUDIO": "0",
         "WWDOS_SOUND_OUT": "null"}   # the stream runs on the clock and plays nowhere


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
    time.sleep(0.08)   # a sound asked for now starts the stream's lead (46 ms) later
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
    R.check(g.get("state", "").startswith("TR-808 drum machine, playing at step"),
            f"look_glance gives the drum machine one sentence: {g.get('state')!r}")
    x(desk, "tr808", id=drm, action="stop")

    # Hearing changes nothing and logs nothing.
    acts = [a for a in acts_since(desk, n0) if "hear" in a["what"]]
    R.check(acts == [], "hear is not logged as an act (it changes nothing)")
    R.check(any(line.find(" ears ") >= 0 for line in desk.screen()), "the synth window shows the ears' meter to the person")


# ---------------------------------------------------------------------------- score

TEST_SCORE = """title Test answer
tempo 480
part person the person at the console
part wib a resident
1 all listen
do tr808 preset electro
do tr808 play
do synth patch pluck
2 person play a phrase
2 wib answer the last phrase
2 all rest two bars
do tr808 stop
"""


def wait_section(desk, sid, n, timeout=6.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        s = x(desk, "event_score", id=sid, action="state")
        if s.get("section", 0) >= n or s.get("done"):
            return s
        time.sleep(0.03)
    raise AssertionError(f"score did not reach section {n}")


def test_score(desk):
    print("\n[score] a person and a resident perform an event score on the synth and the drum machine")
    desk.ipc("close_all")
    time.sleep(0.3)
    sid = x(desk, "open_event_score", text=TEST_SCORE, x=40, y=26, w=78, h=14, actor="wib", source="resident")["id"]
    drm = x(desk, "open_tr808", x=2, y=26, w=118, h=20)["id"]
    syn = x(desk, "open_synth", x=2, y=1, w=60, h=18)["id"]     # opened last: the person's keys go here
    time.sleep(2.2)   # older notes age out of the ears
    g = glance_of(desk, sid) or {}
    R.check(g.get("type") == "event_score", f"look_glance names the type event_score ({g.get('type')})")
    R.check(g.get("state") == 'event score "Test answer", stopped: 4 sections, 7 bars at 480 bpm',
            f"look_glance gives one sentence: {g.get('state')!r}")
    n0 = last_act(desk)

    s = x(desk, "event_score", id=sid, action="start", actor="wib", source="resident")
    R.check(s["running"] and s["section"] == 1 and s["now"]["text"] == "listen" and s["next"]["who"] == "person",
            "wib starts the score: section 1 now, the person next")
    R.check(s["standing"] == "data" and s["from"] == "text", "the score's state is marked data")

    s = wait_section(desk, sid, 2)
    R.check(s["now"]["who"] == "person" and s["bars"] == 2, f"section 2 is the person's phrase ({s['now']})")
    desk.person_type("zxcv", gap=0.06)        # the person plays C D E F

    s = wait_section(desk, sid, 3)
    R.check(s["now"]["kind"] == "answer", "section 3 asks wib to answer")
    ans = s.get("answering", {})
    R.check(ans.get("notes") == ["C-4", "D-4", "E-4", "F-4"] and ans.get("by") == "person 4",
            f"the score gives wib the phrase to answer: {ans}")
    # wib answers: the phrase backwards, a fifth up.
    x(desk, "synth", id=syn, action="notes", notes="C-5 B-4 A-4 G-4", step=100, actor="wib", source="resident")
    time.sleep(0.08)
    h = x(desk, "hear", seconds=0.6, of="synth")
    R.check([e["note"] for e in h["heard"]][:1] == ["C-5"], "wib hears its own answer begin")

    s = wait_section(desk, sid, 4)
    R.check(s["now"]["kind"] == "rest", "section 4 is a rest")
    t0 = time.time()
    while not s.get("done") and time.time() - t0 < 4:
        time.sleep(0.05)
        s = x(desk, "event_score", id=sid, action="state")
    R.check(s.get("done") and not s.get("running"), "the score finishes on the app's clock")
    secs = s["sections"]
    R.check(secs[1]["heard"]["by"] == {"person": 4}, f"section 2 heard 4 notes by the person ({secs[1]['heard']})")
    R.check(secs[2]["heard"]["by"] == {"wib": 4} and secs[2]["heard"]["notes"] == ["C-5", "B-4", "A-4", "G-4"],
            f"section 3 heard wib's answer ({secs[2]['heard']})")
    R.check(secs[3].get("kept") is True, "section 4, the rest, was kept (no synth notes)")
    R.check(secs[1]["heard"]["hits"] > 0, f"the drum machine played under the person ({secs[1]['heard']['hits']} hits)")
    R.check(s["acts"] == ["tr808 preset electro: ok", "tr808 play: ok", "synth patch pluck: ok", "tr808 stop: ok"],
            f"the score's do lines ran ({s['acts']})")
    d = x(desk, "tr808", id=drm, action="state")
    R.check(d["transport"] == "stopped" and d["pattern"] == "A1-A", "the drum machine stopped when the score said so")
    syns = x(desk, "synth", id=syn, action="state")
    R.check(syns["patch"] == "pluck", "the synth took the score's patch")

    acts = acts_since(desk, n0)
    R.check(any(a["actor"] == "wib" and "event_score action=start" in a["what"] for a in acts), "the log has wib starting the score")
    secacts = [a["what"] for a in acts if a["actor"] == "score" and " section " in a["what"]]
    R.check(len(secacts) == 4 and secacts[1].endswith("section 2 of 4: person: play a phrase"),
            f"the log has each section, by the score ({len(secacts)})")
    R.check(any(a["actor"] == "score" and a["what"] == "score do tr808 play" and a["result"] == "ok" for a in acts),
            "the log has the score's do lines, by the score")
    R.check(any(a["actor"] == "score" and a["what"].endswith("finished") for a in acts), "the log has the score finishing")
    R.check(any(a["actor"] == "person" and a["window"] == syn for a in acts), "the log has the person's notes")
    hits = x(desk, "hear", seconds=10, of="tr808", window=drm)["heard"]
    R.check(hits and all(e["who"] == "score" for e in hits), "the ears say the score started the drum machine")

    # A score is data: anything outside the fixed list is refused before a window opens.
    before = len(desk.state()["windows"])
    r1 = xraw(desk, "open_event_score", text="part p x\n1 p go\ndo shell rm -rf ~\n")
    r2 = xraw(desk, "open_event_score", text="exec_command name=close_all\n1 all go\n")
    r3 = xraw(desk, "open_event_score", score="../../etc/passwd")
    R.check(r1.startswith("err score line 3: do acts on tr808 or synth only"), f"a do line outside the list is refused ({r1})")
    R.check(r2.startswith("err score line 1: unknown line"), f"a command in a score is refused ({r2})")
    R.check(r3.startswith("err name is"), f"a score name cannot leave scores/ ({r3})")
    R.check(len(desk.state()["windows"]) == before, "no window opened or closed")

    # The older open_score (the animated Score window) still opens its own window.
    n_before = len(desk.state()["windows"])
    r = xraw(desk, "open_score")
    R.check((r == "ok" or r.startswith('{"ok":true')) and len(desk.state()["windows"]) == n_before + 1,
            "open_score still opens the animated Score window")

    # The worked example in the repo.
    ex = x(desk, "open_event_score", score="call-and-answer")
    R.check(ex.get("title") == "Call and answer" and ex.get("sections") == 8, f"scores/call-and-answer.score opens ({ex})")
    st = x(desk, "event_score", id=ex["id"], action="state")
    R.check(st["from"] == "scores/call-and-answer.score" and [p["name"] for p in st["parts"]] == ["person", "wib"],
            "its parts are the person and wib")


# ---------------------------------------------------------------------------- hands

FAKE_CLAUDE = r"""#!/bin/sh
# A stand-in for the claude CLI: says it is logged in, logs each call, answers with a canned reply.
if [ "$1" = "auth" ]; then echo '{"loggedIn": true, "email": "test@example.invalid", "authMethod": "test"}'; exit 0; fi
for a in "$@"; do printf '%s\n' "<$a>"; done > "$FAKE_DIR/args.txt"
cat "$FAKE_DIR/reply.txt"
"""


def test_hands(_desk):
    print("\n[hands] Scramble's paws and the chat's tools reach the synth and the ears")
    name = os.environ.get("SOUND_TEST_NAME", "soundtest") + "_hands"
    fake = f"/tmp/wibwob_{name}_fake"
    os.makedirs(fake, exist_ok=True)
    with open(f"{fake}/claude", "w") as f:
        f.write(FAKE_CLAUDE)
    os.chmod(f"{fake}/claude", 0o755)
    with open(f"{fake}/reply.txt", "w") as f:
        f.write("nothing yet\n")
    env = {**QUIET, "PATH": f"{fake}:/usr/bin:/bin:/usr/sbin:/sbin", "FAKE_DIR": fake}
    desk = Desk(name, env=env).start()
    try:
        time.sleep(0.8)
        syn = x(desk, "open_synth", x=2, y=1, w=68, h=18, actor="setup")["id"]
        caps = desk.ipc("get_capabilities")
        R.check('"synth"' in caps and '"hear"' in caps and '"open_synth"' in caps,
                "the chat's tui_list_commands (get_capabilities) lists synth, open_synth and hear")
        desk.ipc("open_scramble", actor="setup")
        time.sleep(0.5)
        with open(f"{fake}/reply.txt", "w") as f:
            f.write('a little tune. (=^..^=)\n'
                    'ACT: synth action=notes notes="E-5 . C-5 G-4" step=90 patch=chip\n'
                    'ACT: hear seconds=2\n')
        desk.ipc("scramble_say", text="play me something", actor="eri", source="guest")
        t0 = time.time()
        st = {}
        while time.time() - t0 < 8:
            st = x(desk, "synth", id=syn, action="state")
            if any(r["who"] == "scramble" for r in st["recent"]):
                break
            time.sleep(0.2)
        mine = [r["note"] for r in st.get("recent", []) if r["who"] == "scramble"]
        R.check(mine == ["G-4", "C-5", "E-5"] and st["recent"][0]["patch"] == "chip",
                f"Scramble's ACT line plays the synth as scramble ({mine})")
        args = open(f"{fake}/args.txt").read() if os.path.exists(f"{fake}/args.txt") else ""
        R.check("synth action=notes" in args and "hear seconds=" in args, "her prompt names synth and hear among her paws")
        R.check("<--tools>\n<>" in args, "her model call still has no tools")
        sacts = desk.j("acts", by="scramble")["acts"]
        R.check(any(a["what"].startswith("synth action=notes") and a["result"] == "ok" for a in sacts),
                "the log has scramble playing the synth")
        time.sleep(0.5)   # the phrase's last note starts 270 ms after its first
        h = x(desk, "hear", seconds=3, of="synth")
        R.check([e["note"] for e in h["heard"]] == ["E-5", "C-5", "G-4"] and all(e["who"] == "scramble" for e in h["heard"]),
                f"the ears hear Scramble's tune as hers ({[(e['note'], e['who']) for e in h['heard']]})")

        # The chat's tool client (app_socket.js, what tui_menu_command runs) plays and hears as wob.
        sock_env = {**os.environ, "WWDOS_SOCKET": desk.sock}
        js = os.path.join(REPO, "app/llm/sdk_bridge/app_socket.js")
        p = subprocess.run(["node", js, "wob", "synth", f"id={syn}", "action=notes", "notes=C-3 G-3", "step=80"],
                           capture_output=True, text=True, env=sock_env)
        R.check(p.returncode == 0 and '"played"' in p.stdout, f"the chat's tool plays the synth as wob ({p.stdout.strip()[:60]!r})")
        time.sleep(0.3)
        p = subprocess.run(["node", js, "wob", "hear", "seconds=1", "of=synth"], capture_output=True, text=True, env=sock_env)
        heard = json.loads(p.stdout) if p.stdout.strip().startswith("{") else {}
        R.check(p.returncode == 0 and [e["note"] for e in heard.get("heard", [])][-2:] == ["C-3", "G-3"]
                and heard["heard"][-1]["who"] == "wob", f"the chat's tool hears wob's notes ({p.stdout.strip()[:200]!r})")
        R.check(any(a["actor"] == "wob" and a["what"].startswith("synth action=notes") for a in desk.j("acts", by="wob")["acts"]),
                "the log has wob playing through the chat's tool")
    finally:
        R.check(desk.alive(), "the hands desktop is still running")
        desk.stop()


# ---------------------------------------------------------------------------- stream

def near(t, times, tol):
    return any(abs(t - u) <= tol for u in times)


def test_stream(desk):
    print("\n[stream] one stream and one clock: notes and drum steps sound when the ears say, scores and drums share bars")
    desk.ipc("close_all")
    time.sleep(0.3)
    st = x(desk, "sound", action="state")
    R.check(st["out"] == "null" and st["running"] and abs(st["lead_ms"] - 46.44) < 0.1,
            f"the stream runs (null output in tests), lead 46 ms ({st['out']}, {st['lead_ms']})")
    t0, c0 = time.time(), st["clock_ms"]
    time.sleep(0.5)
    c1 = x(desk, "sound", action="state")["clock_ms"]
    R.check(abs((c1 - c0) - (time.time() - t0) * 1000) < 40, f"the clock follows the stream in real time ({c1 - c0} ms in 500 ms)")

    syn = x(desk, "open_synth", x=2, y=1, w=68, h=18)["id"]
    x(desk, "synth", id=syn, action="audio", on=1)          # into the null stream: mixed, heard by nobody
    time.sleep(2.2)
    x(desk, "synth", id=syn, action="notes", notes="C-4 C-5 C-4 C-5", step=150, dur=60, patch="pluck", actor="wib")
    time.sleep(1.0)
    heard = [e["at_ms"] for e in x(desk, "hear", seconds=2, of="synth")["heard"]]
    cap = x(desk, "sound", action="capture", ms=1800)
    on = cap["onsets_ms"]
    R.check(len(heard) == 4 and all(near(t, on, 3) for t in heard),
            f"each of wib's four notes is in the mix within 3 ms of when the ears say it sounds "
            f"({[round(min(abs(t - u) for u in on), 1) if on else None for t in heard]})")
    R.check(abs((heard[1] - heard[0]) - 150) < 0.1 and abs((heard[3] - heard[0]) - 450) < 0.1,
            "the notes sit exactly 150 ms apart on the clock")

    # The person's keys: each note is in the mix when the ears say (a fixed lead, no per-note wait).
    x(desk, "synth", id=syn, action="patch", value="pluck")
    x(desk, "synth", id=syn, action="gate", value=60)
    time.sleep(0.3)
    desk.person_type("zcbq", gap=0.11)
    time.sleep(0.6)
    mine = [e["at_ms"] for e in x(desk, "hear", seconds=1.5, of="synth")["heard"] if e["who"] == "person"]
    on = x(desk, "sound", action="capture", ms=1500)["onsets_ms"]
    R.check(len(mine) == 4 and all(near(t, on, 3) for t in mine), f"the person's four keys sound when the ears say ({[round(min(abs(t - u) for u in on), 1) if on else None for t in mine]})")

    # Phrases from separate calls share the nine FM channels.
    for i in range(10):
        x(desk, "synth", id=syn, action="note", note="C-3", dur=3000, patch="organ")
    time.sleep(0.2)
    st = x(desk, "sound", action="state")
    R.check(st["fm_busy"] == 9, f"ten notes from ten commands hold nine FM channels ({st['fm_busy']})")
    R.check(st["late"] == 0, f"nothing reached the stream late ({st['late']})")
    x(desk, "synth", id=syn, action="audio", on=0)
    time.sleep(3.2)

    # The TR-808 on the same stream: every drum onset in the mix is a step the ears heard.
    drm = x(desk, "open_tr808", x=2, y=20, w=118, h=26)["id"]
    x(desk, "tr808", id=drm, action="audio", on=1)
    x(desk, "tr808", id=drm, action="load_preset", preset="electro")
    x(desk, "tr808", id=drm, action="play", actor="wob")
    time.sleep(1.6)
    hits = sorted({round(e["at_ms"], 3) for e in x(desk, "hear", seconds=1.5, of="tr808")["heard"]})
    cap = x(desk, "sound", action="capture", ms=1200)
    on = [t for t in cap["onsets_ms"] if t > cap["from_ms"] + 60]
    inwin = [t for t in hits if cap["from_ms"] + 60 < t < cap["to_ms"]]
    R.check(on and all(near(t, hits, 3) for t in on), f"every drum onset in the mix is within 3 ms of a step ({len(on)} onsets)")
    R.check(len(on) >= len(inwin) // 2, f"the mix has an onset for most steps with hits ({len(on)} of {len(inwin)})")
    gaps = {round(b - a, 1) for a, b in zip(hits, hits[1:])}
    R.check(all(abs(g / 125.0 - round(g / 125.0)) < 0.002 for g in gaps), f"the steps are whole 16ths at 120 bpm apart ({sorted(gaps)[:4]})")
    x(desk, "tr808", id=drm, action="stop")
    x(desk, "tr808", id=drm, action="audio", on=0)
    R.check(x(desk, "sound", action="state")["late"] == 0, "no drum hit reached the stream late")

    # A score and the drum machine on one clock: every bar line of the score is a step 1 of the drums.
    score = ("title Bars\ntempo 240\npart wib a resident\n1 all count\ndo tr808 preset electro\ndo tr808 tempo 240\n"
             "do tr808 play\n4 wib play\n1 all rest\ndo tr808 stop\n")
    sid = x(desk, "open_event_score", text=score)["id"]
    x(desk, "event_score", id=sid, action="start")
    time.sleep(6.6)
    s = x(desk, "event_score", id=sid, action="state")
    R.check(s["done"], "the score finished")
    one = [e["at_ms"] for e in x(desk, "hear", seconds=8, of="tr808", window=drm)["heard"] if e["step"] == 0]
    one = sorted(set(one))
    offs = [round((t - s["lead_ms"] - s["started_ms"]) % 1000.0, 3) for t in one]
    offs = [min(o, 1000 - o) for o in offs]
    R.check(len(one) >= 4 and max(offs) < 0.5,
            f"each step 1 of the drums falls on a bar line of the score, to within 0.5 ms ({len(one)} bars, worst {max(offs) if offs else None} ms)")
    R.check(x(desk, "tr808", id=drm, action="state")["transport"] == "stopped", "the drums stopped with the score")


GROUPS = {"synth": test_synth, "ears": test_ears, "score": test_score, "hands": test_hands, "stream": test_stream}


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
