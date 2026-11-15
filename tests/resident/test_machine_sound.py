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
         "WWDOS_SOUND_OUT": "null", "WWDOS_MIDI": "null",
         "WWDOS_SAY": os.path.join(REPO, "tests", "resident", "fake_say.py")}   # a stand-in for macOS say   # the stream runs on the clock and plays nowhere


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
    R.check(h["standing"] == "data" and h["counts"] == {"synth": 0, "tr808": 0, "voice": 0} and "silence" in h["summary"],
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

        # The jam with a model present: a confined call chooses the answer.
        with open(f"{fake}/reply.txt", "w") as f:
            f.write("G-4 . E-4 C-4\n")
        st = x(desk, "jam", action="start", **{"as": "wob"}, wait_ms=500)
        R.check(st["model"] is True, "the jam finds the model CLI on PATH")
        desk.ipc("focus_window", id=syn)
        time.sleep(1.8)
        desk.person_type("zc", gap=0.15)
        st = wait_answers(desk, 1)
        a = st["answers"][0] if st["answers"] else {}
        R.check(a.get("answer") == "G-4 . E-4 C-4" and a.get("chose") == "model", f"the model chose wob's answer ({a})")
        args = open(f"{fake}/args.txt").read() if os.path.exists(f"{fake}/args.txt") else ""
        R.check("<--tools>\n<>" in args and "<--strict-mcp-config>" in args and "<--setting-sources>\n<>" in args
                and "<DATA>C-4 E-4</DATA>" in args, "the call has no tools, no MCP, no settings, and the phrase as data")
        with open(f"{fake}/reply.txt", "w") as f:
            f.write("ACT: close_all\n")
        n_win = len(desk.state()["windows"])
        time.sleep(1.8)
        desk.person_type("v", gap=0.15)
        st = wait_answers(desk, 2)
        a = st["answers"][1] if len(st["answers"]) > 1 else {}
        R.check(a.get("chose", "").startswith("rule") and a.get("answer") == "C-5" and len(desk.state()["windows"]) == n_win,
                f"a reply that is not notes falls back to the rule and does nothing else ({a})")
        x(desk, "jam", action="stop")
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


# ---------------------------------------------------------------------------- drums

def test_drums(desk):
    print("\n[drums] a drum machine started with an empty pattern is heard")
    desk.ipc("close_all")
    time.sleep(0.3)
    drm = x(desk, "open_tr808", x=2, y=2, w=118, h=26)["id"]
    st = x(desk, "tr808", id=drm, action="state")
    R.check(all("x" not in i["steps"] for i in st["instruments"]) and st["start_preset"] == "",
            "a new drum machine opens with an empty pattern")
    x(desk, "tr808", id=drm, action="audio", on=1)    # the test desktop starts it off (WWDOS_TR808_AUDIO=0)
    h0 = x(desk, "sound", action="state")["hits"]
    st = x(desk, "tr808", id=drm, action="play", actor="wob")
    R.check(st["start_preset"] == "classic-house" and any("x" in i["steps"] for i in st["instruments"]),
            f"play on an empty pattern loads classic-house first ({st['start_preset']!r})")
    time.sleep(1.2)
    s = x(desk, "sound", action="state")
    heard = x(desk, "hear", seconds=1, of="tr808", window=drm)["counts"]["tr808"]
    R.check(s["hits"] - h0 > 0 and heard > 0, f"the stream counts the hits it mixed ({s['hits'] - h0}) and the ears hear them ({heard})")
    x(desk, "tr808", id=drm, action="stop")
    x(desk, "tr808", id=drm, action="clear", all=1)
    x(desk, "tr808", id=drm, action="set_step", instrument="bd", step=0, active=1)
    st = x(desk, "tr808", id=drm, action="play")
    R.check([i["steps"] for i in st["instruments"] if i["id"] == "bd"][0] == "x..............." and
            sum("x" in i["steps"] for i in st["instruments"]) == 1, "a pattern with any step on is played as it is")
    x(desk, "tr808", id=drm, action="stop")
    x(desk, "tr808", id=drm, action="audio", on=0)


# ---------------------------------------------------------------------------- tracker

def test_tracker(desk):
    print("\n[tracker] rows written by the person, a resident's commands and its keys; played, saved, loaded, heard")
    desk.ipc("close_all")
    time.sleep(0.3)
    o = x(desk, "open_tracker", channels="fm:bass fm:pluck speaker drums", rows=8, tempo=240, x=2, y=1, w=60, h=14,
          actor="wib", source="resident")
    tid = o["id"]
    st = x(desk, "tracker", id=tid, action="state")
    R.check(st["rows"] == 8 and st["channels"] == ["fm:bass", "fm:pluck", "speaker", "drums"] and st["row_ms"] == 62.5,
            f"open_tracker: 8 rows, four channels, 62.5 ms a row ({st['channels']})")
    d = desk.j("describe", id=tid)
    R.check(d["state"].startswith('tracker "untitled", 4 channels, 1 pattern in an order of 1 at 240 bpm, 0 cells written') and len(d["text"]) == 8,
            f"describe gives a sentence and the rows as text ({d['state']!r})")

    # The person types C D E down channel 1 with the tracker keyboard.
    time.sleep(1.6)
    desk.person_type("zxc", gap=0.08)
    time.sleep(0.3)
    st = x(desk, "tracker", id=tid, action="state")
    R.check([l.split(" | ")[0] for l in st["lines"][:3]] == ["00 C-4 .. ...", "01 D-4 .. ...", "02 E-4 .. ..."],
            f"the person's z x c write C-4 D-4 E-4 down channel 1 ({[l.split(' | ')[0] for l in st['lines'][:3]]})")

    # wib writes the drums and a bass note by command.
    x(desk, "tracker", id=tid, action="write", ch=3, row=0, notes="BD- . SD- . BD- . SD- .", instr="7F", actor="wib", source="resident")
    x(desk, "tracker", id=tid, action="set", row=4, ch=0, cell="G-3 01 G08", actor="wib", source="resident")
    # wob writes with its own keys: an arpeggio on the pluck channel.
    time.sleep(1.6)   # the window stays the person's for share_ms after their last key
    x(desk, "tracker", id=tid, action="cursor", row=4, ch=1, field="note", octave=4, actor="wob", source="resident")
    q(desk, "resident_key", id=tid, text="c", actor="wob")
    x(desk, "tracker", id=tid, action="cursor", row=4, ch=1, field="fx", actor="wob", source="resident")
    q(desk, "resident_key", id=tid, text="a37", actor="wob")
    st = x(desk, "tracker", id=tid, action="state")
    R.check(st["lines"][4] == "04 G-3 01 G08 | E-4 .. A37 | --- .. ... | BD- 7F ...",
            f"row 4 holds wib's bass and drum and wob's keyed arpeggio: {st['lines'][4]!r}")
    R.check(st["lines"][2].endswith("SD- 7F ..."), "wib's snare on row 2")
    acts = desk.j("acts", limit=4000)["acts"]
    R.check(any(a["actor"] == "wib" and "tracker action=write" in a["what"] for a in acts) and
            any(a["actor"] == "wob" and a["window"] == tid and a["what"].startswith("key") for a in acts) and
            any(a["actor"] == "person" and a["window"] == tid for a in acts),
            "the log names the person, wib's commands and wob's keys")

    # Save as plain text, load into a new tracker: the same rows.
    sv = x(desk, "tracker", id=tid, action="save", track="t-soundtest")
    path = os.path.join(REPO, "tracks", "t-soundtest.trk")
    R.check(sv.get("saved") == "tracks/t-soundtest.trk" and os.path.exists(path), "save writes tracks/t-soundtest.trk")
    text = open(path).read() if os.path.exists(path) else ""
    R.check("channels fm:bass fm:pluck speaker drums" in text and "04 G-3 01 G08 | E-4 .. A37" in text, "the file is the rows as plain text")
    t2 = x(desk, "open_tracker", track="t-soundtest", x=70, y=1, w=60, h=14)["id"]
    R.check(x(desk, "tracker", id=t2, action="state")["lines"] == st["lines"], "open_tracker track= loads the same rows")
    R.check(xraw(desk, "tracker", id=t2, action="save", track="../x").startswith("err track is"), "a track name cannot leave tracks/")
    desk.ipc("close_window", id=t2)
    if os.path.exists(path):
        os.remove(path)

    # Play one pass: the ears give back the rows that sounded, in the tracker's own form.
    x(desk, "synth", action="open")
    x(desk, "synth", action="audio", on=1)
    n0 = x(desk, "sound", action="state")["notes"]
    x(desk, "tracker", id=tid, action="loop", on=0)
    x(desk, "tracker", id=tid, action="play", actor="wib")
    time.sleep(1.0)
    st = x(desk, "tracker", id=tid, action="state")
    h = x(desk, "hear", window=tid, seconds=1.5)
    sounding = [l for l in st["lines"] if any(c.strip()[:3] not in ("---", "===") for c in l[3:].split(" | "))]
    R.check(not st["playing"] and h.get("tracker_rows") == sounding,
            f"the ears' rows are the tracker's rows that sounded ({len(h.get('tracker_rows', []))} of {len(sounding)})")
    R.check(x(desk, "sound", action="state")["notes"] - n0 == 3 + 1 + 3, "the stream mixed the pass's notes (C D E, G-3, the arpeggio's three)")
    x(desk, "tracker", id=tid, action="loop", on=1)
    x(desk, "tracker", id=tid, action="play")
    time.sleep(1.3)
    st = x(desk, "tracker", id=tid, action="state")
    hits = x(desk, "hear", window=tid, seconds=1.2, of="tr808")["counts"]["tr808"]
    R.check(st["playing"] and st["loop"] and hits >= 6, f"with loop on it plays round ({hits} drum hits in 1.2 s)")
    x(desk, "tracker", id=tid, action="stop")
    x(desk, "synth", action="audio", on=0)


# ---------------------------------------------------------------------------- midi

def test_midi(desk):
    print("\n[midi] MIDI in plays the synth with real releases; the TR-808 and the tracker send clock and notes out")
    desk.ipc("close_all")
    time.sleep(0.3)
    st = x(desk, "midi", action="state")
    R.check(st["device"] == "null", f"the test desktop's MIDI device is null ({st['device']})")
    mon = x(desk, "open_midi", x=70, y=1, w=64, h=16)["id"]
    syn = x(desk, "open_synth", x=2, y=1, w=68, h=18)["id"]
    x(desk, "synth", id=syn, action="audio", on=1)
    x(desk, "synth", id=syn, action="patch", value="organ")
    time.sleep(2.2)

    x(desk, "midi", action="in", data="90 3C 64")
    time.sleep(0.1)
    h = x(desk, "hear", seconds=1, of="synth")
    R.check(any(s["note"] == "C-4" and s["who"] == "person" for s in h["sounding"]), "a MIDI note on plays C-4 on the synth, as the person's")
    R.check(x(desk, "sound", action="state")["fm_busy"] == 1, "the note holds an FM channel while the key is down")
    time.sleep(0.3)
    x(desk, "midi", action="in", data="80 3C 00")
    time.sleep(0.3)
    h = x(desk, "hear", seconds=2, of="synth")
    c4 = [e for e in h["heard"] if e["note"] == "C-4"]
    R.check(c4 and 360 <= c4[-1]["dur_ms"] <= 460 and h["sounding"] == [],
            f"the note off ends it: C-4 lasted as long as the key was held ({c4[-1]['dur_ms'] if c4 else None} ms)")
    R.check(x(desk, "sound", action="state")["fm_busy"] == 0, "the channel is free after the release")
    x(desk, "midi", action="in", data="90 40 50")
    time.sleep(0.15)
    x(desk, "midi", action="in", data="90 40 00")       # velocity 0 is a note off
    time.sleep(0.2)
    e4 = [e for e in x(desk, "hear", seconds=1, of="synth")["heard"] if e["note"] == "E-4"]
    R.check(e4 and e4[-1]["dur_ms"] < 300, f"a note on with velocity 0 lets the key go ({e4[-1]['dur_ms'] if e4 else None} ms)")
    acts = desk.j("acts", by="person")["acts"]
    R.check(any(a["what"].startswith("midi note on C-4 v100 ch1") and a["window"] == syn for a in acts), "the log has the person's MIDI note")
    x(desk, "synth", id=syn, action="audio", on=0)

    # TR-808 out: start, clock ticks 24 a beat on the one clock, drums on channel 10, stop.
    drm = x(desk, "open_tr808", x=2, y=20, w=118, h=26)["id"]
    x(desk, "tr808", id=drm, action="load_preset", preset="electro")
    n0 = x(desk, "midi", action="state")["out"]
    x(desk, "tr808", id=drm, action="play")
    time.sleep(1.0)
    x(desk, "tr808", id=drm, action="stop")
    st = x(desk, "midi", action="log", n=1000)
    out = [m for m in st["log"] if m["dir"] == "out" and m["from"] == "tr808"][-(st["out"] - n0):]
    clocks = [m["ms"] for m in out if m["hex"] == "F8"]
    gaps = [b - a for a, b in zip(clocks, clocks[1:])]
    R.check(out and out[0]["hex"] == "FA" and out[-1]["hex"] == "FC", "the drum machine sends start first and stop last")
    R.check(len(clocks) >= 40 and all(abs(g - 20.833) < 0.01 for g in gaps),
            f"clock ticks are 24 a beat at 120 bpm, 20.833 ms apart on the clock ({len(clocks)} ticks; off: {[round(g, 3) for g in gaps if abs(g - 20.833) >= 0.01][:5]})")
    drums = [m for m in out if m["hex"].startswith("99 ")]
    bd = [m for m in drums if m["hex"].startswith("99 24 ")]
    heard_bd = [e for e in x(desk, "hear", seconds=3, of="tr808", window=drm)["heard"] if e["voice"] == "bd"]
    R.check(drums and len(bd) == len(heard_bd) and all(abs(a["ms"] - b["at_ms"]) < 0.01 for a, b in zip(bd, heard_bd)),
            f"each bass drum goes out as note 36 on channel 10 at the time it sounds ({len(bd)} out, {len(heard_bd)} heard)")

    # Tracker out: a note on channel 1 and its note off one gate later.
    song = "channels fm:bass drums\n00 C-4 01 G08 | BD- 7F ...\n01 --- .. ... | --- .. ...\n"
    tid = x(desk, "open_tracker", text=song)["id"]
    x(desk, "tracker", id=tid, action="loop", on=0)
    n1 = x(desk, "midi", action="state")["out"]
    x(desk, "tracker", id=tid, action="play")
    time.sleep(0.6)
    st = x(desk, "midi", action="log", n=1000)
    tall = [m for m in st["log"] if m["dir"] == "out" and m["from"] == "tracker"]
    starts = [i for i, m in enumerate(tall) if m["hex"] == "FA"]
    tout = tall[starts[-1]:] if starts else []           # from this play's start
    on = [m for m in tout if m["hex"] == "90 3C 64"]
    off = [m for m in tout if m["hex"] == "80 3C 00"]
    R.check(on and off and abs((off[-1]["ms"] - on[-1]["ms"]) - 62.5) < 0.01,
            f"the tracker sends C-4 on channel 1 and its note off 62.5 ms later (G08 at 120 bpm) {[(m['hex'], round(m['ms'],1)) for m in tout if m['hex'] != 'F8']}")
    R.check(any(m["hex"] == "99 24 7F" for m in tout) and sum(m["hex"] == "F8" for m in tout) == 12,
            "and its drum on channel 10, and 6 clock ticks a row")
    d = desk.j("describe", id=mon)
    R.check(d["state"].startswith("MIDI monitor, device null,") and any(l.startswith("IN ") for l in d["text"]) and any(l.startswith("OUT") for l in d["text"]),
            f"the MIDI window says what arrived and what left ({d['state']!r})")


# ---------------------------------------------------------------------------- jam

def wait_answers(desk, n, timeout=8.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        st = x(desk, "jam", action="state")
        if len(st["answers"]) >= n:
            return st
        time.sleep(0.1)
    return x(desk, "jam", action="state")


def test_jam(desk):
    print("\n[jam] a resident answers the person's last phrase on the synth, by rule")
    desk.ipc("close_all")
    time.sleep(0.3)
    syn = x(desk, "open_synth", x=2, y=1, w=68, h=18)["id"]
    time.sleep(2.2)
    st = x(desk, "jam", action="start", **{"as": "wib"}, wait_ms=600)
    R.check(st["on"] and st["as"] == "wib" and st["model"] is False, "jam starts as wib, with no model on this desktop")
    desk.person_type("zxcv", gap=0.15)                    # C D E F
    st = wait_answers(desk, 1)
    a = st["answers"][0] if st["answers"] else {}
    R.check(a.get("phrase") == "C-4 D-4 E-4 F-4" and a.get("answer") == "C-5 B-4 A-4 G-4" and a.get("chose") == "rule",
            f"after the person stops, wib answers C D E F with C-5 B-4 A-4 G-4 by rule ({a})")
    R.check(140 <= a.get("step_ms", 0) <= 260, f"in the person's rhythm ({a.get('step_ms')} ms a note)")
    time.sleep(1.0)
    h = x(desk, "hear", seconds=4, of="synth")
    wib = [e["note"] for e in h["heard"] if e["who"] == "wib"]
    R.check(wib == ["C-5", "B-4", "A-4", "G-4"], f"the ears hear wib's answer ({wib})")
    acts = desk.j("acts", by="wib")["acts"]
    R.check(any(x_["what"].startswith("synth action=notes") and x_["window"] == syn for x_ in acts), "the log has wib playing the answer")
    # Only the person's phrase is answered: wib's answer starts no second answer.
    time.sleep(1.2)
    R.check(len(x(desk, "jam", action="state")["answers"]) == 1, "wib does not answer its own notes")
    time.sleep(1.6)
    desk.person_type("cx", gap=0.15)
    st = wait_answers(desk, 2)
    R.check(len(st["answers"]) == 2 and st["answers"][1]["phrase"] == "E-4 D-4", f"the next phrase gets its own answer ({st['answers'][1:] })")
    x(desk, "jam", action="stop")


# ---------------------------------------------------------------------------- patterns

def test_patterns(desk):
    print("\n[patterns] patterns and an order list, copy and paste, mute and solo, tempo and speed effects")
    desk.ipc("close_all")
    time.sleep(0.3)
    old = x(desk, "open_tracker", track="arp-and-bass", x=60, y=1, w=60, h=12)
    st = x(desk, "tracker", id=old["id"], action="state")
    R.check(st["patterns"] == [16] and st["order"] == [0], f"a file in the first format loads as one pattern ({st['patterns']}, order {st['order']})")
    desk.ipc("close_window", id=old["id"])
    tid = x(desk, "open_tracker", channels="fm:bass drums", rows=4, tempo=240, x=2, y=1, w=60, h=14)["id"]
    x(desk, "tracker", id=tid, action="write", ch=1, row=0, notes="BD- CH- SD- CH-")
    x(desk, "tracker", id=tid, action="set", row=0, ch=0, cell="C-3 01 ...")
    st = x(desk, "tracker", id=tid, action="new_pattern", rows=2)
    R.check(st["patterns"] == [4, 2] and st["edit_pattern"] == 1, "new_pattern adds pattern 1, two rows long, and edits it")
    x(desk, "tracker", id=tid, action="set", pat=1, row=0, ch=1, cell="BD- .. F78")      # F78: tempo 120 from here
    x(desk, "tracker", id=tid, action="set", pat=1, row=1, ch=1, cell="SD-")
    x(desk, "tracker", id=tid, action="order", value="0 1 0")
    x(desk, "tracker", id=tid, action="loop", on=0)
    x(desk, "tracker", id=tid, action="play")
    time.sleep(1.6)
    st = x(desk, "tracker", id=tid, action="state")
    h = x(desk, "hear", window=tid, seconds=3)
    rows = h.get("tracker_rows", [])
    p0 = ["00 C-3 01 ... | BD- .. ...", "01 --- .. ... | CH- .. ...", "02 --- .. ... | SD- .. ...", "03 --- .. ... | CH- .. ..."]
    p1 = ["00 --- .. ... | BD- .. F78", "01 --- .. ... | SD- .. ..."]
    R.check(not st["playing"] and rows == p0 + p1 + p0, f"the song plays pattern 0, 1, 0 in order and the ears give back its rows ({len(rows)} rows)")
    hits = [e["at_ms"] for e in h["heard"] if e["source"] == "tr808"]
    gaps = [round(b - a, 3) for a, b in zip(hits, hits[1:])]
    R.check(gaps[:4] == [62.5, 62.5, 62.5, 62.5] and gaps[4:9] == [125.0] * 5,
            f"rows are 62.5 ms at 240 bpm until F78 sets 120 bpm, then 125 ms ({gaps})")
    R.check(st["tempo_now"] == 120, "the tempo stays where the effect left it")

    # Mute and solo.
    x(desk, "tracker", id=tid, action="tempo", value=240)
    x(desk, "tracker", id=tid, action="mute", ch=1, on=1)
    x(desk, "tracker", id=tid, action="play_pattern", pat=0)
    time.sleep(0.5)
    x(desk, "tracker", id=tid, action="stop")
    h = x(desk, "hear", window=tid, seconds=0.5)
    R.check(h["counts"]["tr808"] == 0 and h["counts"]["synth"] >= 1, "with the drums muted only the bass sounds")
    x(desk, "tracker", id=tid, action="mute", ch=1, on=0)
    x(desk, "tracker", id=tid, action="solo", ch=1, on=1)
    time.sleep(0.6)
    x(desk, "tracker", id=tid, action="play_pattern", pat=0)
    time.sleep(0.5)
    x(desk, "tracker", id=tid, action="stop")
    h = x(desk, "hear", window=tid, seconds=0.5)
    R.check(h["counts"]["synth"] == 0 and h["counts"]["tr808"] >= 4, "with the drums soloed only the drums sound")
    x(desk, "tracker", id=tid, action="solo", ch=1, on=0)

    # Copy and paste a block.
    x(desk, "tracker", id=tid, action="copy", pat=0, row=0, rows=2, ch=0, chs=2)
    st = x(desk, "tracker", id=tid, action="paste", pat=1, row=0, ch=0)
    R.check(x(desk, "tracker", id=tid, action="state", pat=1)["clipboard"] == [2, 2], "copy takes a block of 2 rows by 2 channels")
    x(desk, "tracker", id=tid, action="pattern", value=1)
    st = x(desk, "tracker", id=tid, action="state")
    R.check(st["lines"] == ["00 C-3 01 ... | BD- .. ...", "01 --- .. ... | CH- .. ..."], f"paste puts it in pattern 1 ({st['lines']})")
    x(desk, "tracker", id=tid, action="copy", pat=0, row=0, rows=1, ch=1, chs=1)
    x(desk, "tracker", id=tid, action="paste", pat=1, row=1, ch=0)
    R.check(x(desk, "tracker", id=tid, action="state")["lines"][1].startswith("01 --- .. ..."),
            "a drum column pasted on a note channel changes nothing")

    # The person's keys: > edits the next pattern, Ctrl+B mutes the channel under the cursor.
    x(desk, "tracker", id=tid, action="pattern", value=0)
    time.sleep(0.3)
    desk.person_text(">")
    time.sleep(0.2)
    desk.person_keys("C-e")
    time.sleep(0.2)
    desk.person_keys("C-t")
    time.sleep(0.2)
    st = x(desk, "tracker", id=tid, action="state")
    c = st["cursor"]["ch"]
    R.check(st["edit_pattern"] == 1 and st["mute"][c] and st["solo"][c],
            f"the person's >, Ctrl+E and Ctrl+T edit pattern 1, mute and solo the channel ({st['edit_pattern']}, {st['mute']}, {st['solo']})")
    txt = x(desk, "tracker", id=tid, action="text")["text"]
    R.check("order 0 1 0" in txt and "pattern 1" in txt, "the song text holds the order and both patterns")


# ---------------------------------------------------------------------------- voice

def test_voice(desk):
    print("\n[voice] the residents sing: words on notes, rendered once with a stand-in for say, heard with who sang")
    desk.ipc("close_all")
    time.sleep(0.3)
    log = f"/tmp/wibwob_{desk.name}_say.log"
    st = x(desk, "voice", action="state")
    sing = {s["who"]: s for s in st["singers"]}
    R.check(sing["wib"]["voice"] == "Sandy (English (UK))" and sing["wob"]["voice"] == "Grandpa (English (UK))"
            and not sing["wib"]["fallback"] and not sing["wob"]["fallback"], "wib sings as Sandy, wob as Grandpa")
    r1 = x(desk, "voice", action="render", who="wob", word="hello", note="A-2")
    R.check(abs(r1["spoken_hz"] - 115) < 2 and abs(r1["hz"] - 110) < 2,
            f"wob's hello (spoken at {r1['spoken_hz']:.1f} Hz) is sung on A-2 at {r1['hz']:.1f} Hz")
    r2 = x(desk, "voice", action="render", who="wob", word="hello", note="E-3")
    R.check(r2["renders"] == r1["renders"] and abs(r2["hz"] - 164.8) < 3, "the same word on another note is not rendered again")
    syn = x(desk, "open_synth", x=2, y=1, w=68, h=18)["id"]
    x(desk, "synth", id=syn, action="audio", on=1)
    h0 = x(desk, "sound", action="state")["hits"]
    x(desk, "synth", id=syn, action="sing", who="wib", words="ma ri a", notes="C-4 E-4 G-4", step=200, actor="wib", source="resident")
    time.sleep(0.9)
    h = x(desk, "hear", seconds=2, of="voice")
    words = [(e["word"], e["note"], e["who"]) for e in h["heard"]]
    R.check(words == [("ma", "C-4", "wib"), ("ri", "E-4", "wib"), ("a", "G-4", "wib")], f"the ears hear the words, their notes and who sang ({words})")
    R.check('wib sang "ma ri a"' in h["summary"], f"summary: {h['summary']!r}")
    R.check(x(desk, "sound", action="state")["hits"] - h0 == 3, "the stream mixed the three sung words")
    gaps = [round(b["at_ms"] - a["at_ms"], 3) for a, b in zip(h["heard"], h["heard"][1:])]
    R.check(gaps == [200.0, 200.0], f"on the one clock, 200 ms apart ({gaps})")
    # A tracker voice channel.
    song = "channels voice:wob drums\n00 C-3 la     ... | BD- .. ...\n01 G-3 li     ... | --- .. ...\n"
    tid = x(desk, "open_tracker", text=song, x=2, y=20, w=60, h=10)["id"]
    x(desk, "tracker", id=tid, action="loop", on=0)
    x(desk, "tracker", id=tid, action="play")
    time.sleep(0.8)
    h = x(desk, "hear", window=tid, seconds=2)
    R.check(h.get("tracker_rows") == ["00 C-3 la     ... | BD- .. ...", "01 G-3 li     ... | --- .. ..."],
            f"the tracker's voice rows come back from the ears ({h.get('tracker_rows')})")
    sung = [(e["word"], e["who"]) for e in h["heard"] if e["source"] == "voice"]
    R.check(sung == [("la", "wob"), ("li", "wob")], f"sung by wob ({sung})")
    lines = open(log).read().splitlines() if os.path.exists(log) else []
    R.check(sorted(set(lines)) == sorted(lines) and len(lines) == 6, f"say ran once per word and singer ({len(lines)} renders: {lines})")
    R.check(xraw(desk, "synth", id=syn, action="sing", words="rm;-rf", who="wib").startswith("err"), "a word that is not letters is refused")
    x(desk, "tracker", id=tid, action="cursor", row=1, ch=0, field="instr")
    desk.ipc("focus_window", id=tid)
    time.sleep(0.3)
    desk.person_type("lo", gap=0.1)
    R.check(x(desk, "tracker", id=tid, action="state")["lines"][1].startswith("01 G-3 lilo"), "the person types letters into a voice cell's word")
    x(desk, "synth", id=syn, action="audio", on=0)

    # A desktop where Grandpa is missing: wob sings as the next English voice and says so.
    alt = Desk(desk.name + "_alt", env={**QUIET, "FAKE_SAY_VOICES": "Sandy (English (UK)),Reed (English (UK))"}).start()
    try:
        st = alt.j("exec_command", name="voice", action="state")
        wob = [s for s in st["singers"] if s["who"] == "wob"][0]
        R.check(wob["voice"] == "Reed (English (UK))" and wob["fallback"], f"with no Grandpa, wob sings as Reed and says it is a stand-in ({wob})")
    finally:
        alt.stop()


# ---------------------------------------------------------------------------- midi in: clock, controllers, bend

def test_midiin(desk):
    print("\n[midiin] MIDI clock sets the tempo, start and stop drive the players, controllers and pitch bend reach the synth")
    desk.ipc("close_all")
    time.sleep(0.3)
    drm = x(desk, "open_tr808", x=2, y=20, w=118, h=26)["id"]
    x(desk, "tr808", id=drm, action="load_preset", preset="electro")
    tid = x(desk, "open_tracker", channels="fm:bass drums", rows=4, x=70, y=1, w=50, h=10)["id"]
    x(desk, "tracker", id=tid, action="set", row=0, ch=1, cell="BD-")
    syn = x(desk, "open_synth", x=2, y=1, w=68, h=18)["id"]
    x(desk, "midi", action="in", data="F8", count=48, every_ms=20)          # 24 a beat, 20 ms apart: 125 bpm
    st = x(desk, "midi", action="state")
    R.check(st["clock_bpm"] == 125 and st["follow"], f"48 clock ticks 20 ms apart read as 125 bpm ({st['clock_bpm']})")
    R.check(x(desk, "tr808", id=drm, action="state")["tempo"] == 125 and x(desk, "tracker", id=tid, action="state")["tempo"] == 125,
            "the drum machine and the tracker take the clock's tempo")
    x(desk, "midi", action="in", data="FA")
    R.check(x(desk, "tr808", id=drm, action="state")["transport"] == "playing" and x(desk, "tracker", id=tid, action="state")["playing"],
            "MIDI start starts the drum machine and the tracker")
    time.sleep(0.4)
    x(desk, "midi", action="in", data="FC")
    R.check(x(desk, "tr808", id=drm, action="state")["transport"] == "stopped" and not x(desk, "tracker", id=tid, action="state")["playing"],
            "MIDI stop stops them")
    x(desk, "midi", action="follow", on=0)
    x(desk, "midi", action="in", data="F8", count=48, every_ms=25)
    R.check(x(desk, "tr808", id=drm, action="state")["tempo"] == 125, "with follow off the clock changes nothing")
    x(desk, "midi", action="follow", on=1)

    # Controllers.
    x(desk, "midi", action="in", data="B0 01 7F")
    x(desk, "midi", action="in", data="B0 4A 40")
    x(desk, "midi", action="in", data="B0 07 40")
    c = x(desk, "midi", action="state")["controls"]
    R.check(c["mod"] == 1 and abs(c["feedback"] - 64 / 127) < 1e-3 and abs(c["volume"] - 64 / 127) < 1e-3,
            f"mod wheel, CC74 and CC7 set the synth's index, feedback and volume ({c})")
    x(desk, "midi", action="in", data="90 3C 7F")
    time.sleep(0.1)
    x(desk, "midi", action="in", data="80 3C 00")
    e = [h for h in x(desk, "hear", seconds=1, of="synth")["heard"] if h["note"] == "C-4"]
    R.check(e and abs(e[-1]["level"] - 0.5) < 0.03, f"a MIDI note at velocity 127 plays at half volume after CC7=64 ({e[-1]['level'] if e else None})")
    x(desk, "midi", action="in", data="B0 07 7F")
    x(desk, "midi", action="in", data="B0 01 00")
    x(desk, "midi", action="in", data="B0 0B 7F")
    m = x(desk, "mosh")
    R.check(m.get("amount") == 100, f"CC11 sets the desk mosh amount through the command table ({m.get('amount')})")
    acts = desk.j("acts", by="person")["acts"]
    R.check(any("midi cc 11=127" in a["what"] and "desk mosh amount (ok)" in a["what"] for a in acts), "the log has the person's controller")

    # Pitch bend: a held speaker A-4 rises two semitones.
    x(desk, "synth", id=syn, action="audio", on=1)
    x(desk, "synth", id=syn, action="voice", value="pcspk")
    x(desk, "midi", action="in", data="90 45 64")
    time.sleep(0.4)
    hz0 = x(desk, "sound", action="capture", ms=300)["hz_last_200ms"]
    x(desk, "midi", action="in", data="E0 7F 7F")
    time.sleep(0.4)
    hz1 = x(desk, "sound", action="capture", ms=300)["hz_last_200ms"]
    x(desk, "midi", action="in", data="80 45 00")
    x(desk, "midi", action="in", data="E0 00 40")
    R.check(abs(hz0 - 440) <= 10 and abs(hz1 - 493.9) <= 12, f"pitch bend up takes the held A-4 from {hz0} Hz to {hz1} Hz (494 wanted)")
    R.check(x(desk, "midi", action="state")["controls"]["bend"] == 0, "the bend wheel back at the centre is no bend")
    x(desk, "synth", id=syn, action="audio", on=0)


GROUPS = {"midiin": test_midiin, "voice": test_voice, "patterns": test_patterns, "jam": test_jam, "midi": test_midi, "tracker": test_tracker, "drums": test_drums, "synth": test_synth, "ears": test_ears, "score": test_score, "hands": test_hands, "stream": test_stream}


def main():
    groups = [g.lower() for g in sys.argv[1:]] or list(GROUPS)
    name = os.environ.get("SOUND_TEST_NAME", "soundtest")
    QUIET["FAKE_SAY_LOG"] = f"/tmp/wibwob_{name}_say.log"
    QUIET["TMPDIR"] = f"/tmp/wibwob_{name}_tmp"            # a fresh voice cache for each run
    subprocess.run(["rm", "-rf", QUIET["TMPDIR"]])
    os.makedirs(QUIET["TMPDIR"])
    if os.path.exists(QUIET["FAKE_SAY_LOG"]):
        os.remove(QUIET["FAKE_SAY_LOG"])
    desk = Desk(name, env=QUIET).start()
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
