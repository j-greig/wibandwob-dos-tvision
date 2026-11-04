#!/usr/bin/env python3
"""Contract tests for the stage endpoints (tools/api_server/show_stage.py) and the EGA theme route.

Runs without the C++ app: the main desktop's IPC is a fake that keeps window rects, and each
nested desktop is a fake socket that answers like an inner WIBWOB-DOS.
    tools/api_server/venv/bin/python -m unittest tests/test_show_stage.py -v

Endpoints covered (one test class each): POST /show/film, POST /show/nest (+ /show/inner/{instance}/{verb},
/show/nest/close_all), POST /theme/variant {"variant":"ega"}, window colour via POST /windows and /props.
Live check: tools/api_server/README.md § Show endpoints (curl lines).
"""
import gzip
import json
import os
import sys
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
os.environ.setdefault("WIBWOB_SHOW_TMP", tempfile.mkdtemp(prefix="wwdos-stage-test-"))
os.environ.pop("WIBWOB_CAPTURE_TMUX", None)

from fastapi.testclient import TestClient  # noqa: E402

import tools.api_server.controller as controller_mod  # noqa: E402
import tools.api_server.show as show_mod  # noqa: E402
import tools.api_server.show_stage as stage_mod  # noqa: E402


class FakeApp:
    """One desktop's IPC (main desktop or a nested one)."""

    def __init__(self, name, cols=192, rows=53):
        self.name = name
        self.cols, self.rows = cols, rows
        self.wins = {}
        self.n = 0
        self.calls = []
        self.texts = {}
        self.chrome = {}
        self.theme = "monochrome"

    def add(self, x, y, w, h, title="", typ="frame_player"):
        self.n += 1
        wid = f"w{self.n}"
        self.wins[wid] = dict(id=wid, title=title, type=typ, x=x, y=y, width=w, height=h)
        return wid

    def __call__(self, cmd, kv=None):
        kv = {k: str(v) for k, v in (kv or {}).items()}
        self.calls.append((cmd, kv))
        if cmd == "get_state":
            return json.dumps({"theme_variant": self.theme, "windows": list(self.wins.values())})
        if cmd == "get_canvas_size":
            return json.dumps({"width": self.cols, "height": self.rows})
        if cmd == "close_window":
            self.wins.pop(kv["id"], None)
            return '{"success":true}'
        if cmd == "close_all":
            self.wins.clear()
            return "ok"
        if cmd == "move_many":
            for item in kv["moves"].split(";"):
                f = item.split(",")
                self.wins[f[0]].update(x=int(f[1]), y=int(f[2]))
            return '{"success":true}'
        if cmd == "send_text":
            self.texts[kv["id"]] = (self.texts.get(kv["id"], "") if kv["mode"] == "append" else "") + kv["content"]
            return "ok"
        if cmd == "create_window":
            if kv.get("colour") == "pink":
                return '{"success":true,"id":"x","colour_error":"err unknown colour"}'
            wid = self.add(int(kv.get("x", 1)), int(kv.get("y", 1)), int(kv.get("w", 20)), int(kv.get("h", 8)),
                           kv.get("title", ""), kv["type"])
            self.wins[wid].update({k: kv[k] for k in ("colour", "ink", "text") if k in kv})
            return json.dumps({"success": True, "id": wid})
        if cmd == "exec_command":
            name = kv["name"]
            if name == "open_terminal":
                wid = self.add(int(kv.get("x", 0)), int(kv.get("y", 0)), int(kv.get("w", 80)),
                               int(kv.get("h", 24)), kv.get("title", ""), "terminal")
                self.wins[wid]["exec"] = kv.get("exec", "")
                self.wins[wid]["hold"] = kv.get("hold", "")
                return json.dumps({"ok": True, "id": wid})
            if name == "open_primer":
                wid = self.add(int(kv.get("x", 1)), int(kv.get("y", 1)), int(kv.get("w", 20)), int(kv.get("h", 8)),
                               kv.get("title", ""))
                self.wins[wid].update(path=kv["path"], colour=kv.get("colour"), ink=kv.get("ink"))
                return "ok"
            if name == "window_colour":
                if kv.get("colour") not in ("", "blue", "cyan", "grey", "black", "magenta", "red", "green",
                                            "brown", "default"):
                    return "err unknown colour (blue|cyan|grey|black|magenta|red|green|brown)"
                self.wins.setdefault(kv["id"], {"id": kv["id"]}).update(colour=kv.get("colour"), ink=kv.get("ink"))
                return "ok"
            if name == "set_theme_variant":
                if kv["variant"] not in ("monochrome", "dark_pastel", "ega"):
                    return "err invalid theme variant"
                self.theme = kv["variant"]
                return "ok"
            if name == "chrome_set":
                self.chrome[kv["part"]] = kv.get("text", "")
                return "ok"
            if name == "window_title":
                self.wins[kv["id"]]["title"] = kv["title"]
                return "ok"
            return "ok"
        return "ok"


class StageBase(unittest.TestCase):
    def setUp(self):
        self.main = FakeApp("main")
        self.inner = {}                       # sock path -> FakeApp
        self._orig = (show_mod.send_cmd, controller_mod.send_cmd, stage_mod.ipc_at, stage_mod.sock_alive,
                      stage_mod.sock_pids, stage_mod.Stage._wait_sock, stage_mod.Stage.adopt)
        show_mod.send_cmd = self.main
        controller_mod.send_cmd = self.main
        stage_mod.ipc_at = self.fake_ipc_at
        stage_mod.sock_alive = lambda sock: sock in self.inner
        stage_mod.sock_pids = lambda sock: []
        test = self

        async def wait_sock(stage, sock, boot_s):
            test.inner[sock] = FakeApp(sock, cols=60, rows=25)
            return True
        stage_mod.Stage._wait_sock = wait_sock
        stage_mod.Stage.adopt = lambda stage: None
        show_mod.capture_text = lambda: ""
        self.tmp = Path(tempfile.mkdtemp(prefix="wwdos-tapes-"))
        os.environ["WWDOS_TAPES"] = str(self.tmp)
        nest_bin = self.tmp / "wwdos"
        nest_bin.write_text("#!/bin/sh\n")
        os.environ["WWDOS_NEST_BIN"] = str(nest_bin)
        from tools.api_server.main import make_app
        self.client = TestClient(make_app())
        self.client.__enter__()               # keep one event loop alive for background tasks

    def tearDown(self):
        self.client.__exit__(None, None, None)
        (show_mod.send_cmd, controller_mod.send_cmd, stage_mod.ipc_at, stage_mod.sock_alive,
         stage_mod.sock_pids, stage_mod.Stage._wait_sock, stage_mod.Stage.adopt) = self._orig
        os.environ.pop("WWDOS_TAPES", None)
        os.environ.pop("WWDOS_NEST_BIN", None)

    def fake_ipc_at(self, sock, cmd, kv=None, timeout=5.0):
        app = self.inner.get(sock)
        if app is None:
            raise OSError(f"no socket {sock}")
        return app(cmd, kv)

    def recent(self, typ=None):
        evs = self.client.get("/events/recent", params={"n": 2000}).json()["events"]
        return [e for e in evs if typ is None or e["type"] == typ]


class FilmTest(StageBase):
    def make_tape(self, slug="tiny", cols=10, rows=4, fps=4, duration=0.4):
        with gzip.open(self.tmp / f"{slug}.tape.gz", "wt") as z:
            z.write(json.dumps({"cols": cols, "rows": rows, "fps": fps, "frames": 2}) + "\n")
            z.write("\x1b[Hframe0\n\x1b[Hframe1\n")
        (self.tmp / f"{slug}.mp3").write_bytes(b"ID3")
        idx = {"tapes": [{"slug": slug, "title": "a tiny tape", "cols": cols, "rows": rows, "fps": fps,
                          "duration": duration, "file": f"{slug}.tape.gz", "audio": f"{slug}.mp3"}]}
        (self.tmp / "INDEX.json").write_text(json.dumps(idx))

    def test_film_opens_a_centred_terminal_running_the_tape_at_an_epoch(self):
        self.make_tape()
        t0 = time.time()
        r = self.client.post("/show/film", json={"tape": "tiny", "lead_ms": 1000, "actor": "wib"})
        self.assertEqual(r.status_code, 200, r.text)
        out = r.json()
        self.assertEqual(set(out) >= {"window_id", "at", "duration", "fps", "audio", "title"}, True)
        self.assertAlmostEqual(out["at"], t0 + 1.0, delta=0.3)
        self.assertEqual(out["audio"], str((self.tmp / "tiny.mp3").resolve()))
        self.assertEqual(out["title"], "a tiny tape")
        win = self.main.wins[out["window_id"]]
        self.assertEqual((win["width"], win["height"]), (12, 6))              # cols+2 x rows+2
        self.assertEqual((win["x"], win["y"]), ((192 - 12) // 2, (52 - 6) // 2))
        self.assertIn("tape_play.py", win["exec"])
        self.assertIn(f"--at {out['at']:.3f}", win["exec"])
        self.assertTrue(win["exec"].startswith("exec python3 "))
        self.assertEqual(win["hold"], "1")                                     # keeps the last frame
        start = self.recent("film")[0]
        self.assertEqual(start["data"]["phase"], "start")
        self.assertEqual(start["data"]["tape"], "tiny")
        self.assertEqual(start["data"]["at"], out["at"])
        self.assertEqual(start["data"]["audio"], out["audio"])
        self.assertEqual(start["window"]["id"], out["window_id"])
        self.assertEqual(start["actor"], "wib")

    def test_film_end_event_and_close_after(self):
        self.make_tape(duration=0.2)
        r = self.client.post("/show/film", json={"tape": "tiny", "lead_ms": 0, "close_after": True})
        wid = r.json()["window_id"]
        self.assertEqual(self.main.wins[wid]["hold"], "0")
        for _ in range(40):
            if any(e["data"].get("phase") == "end" for e in self.recent("film")):
                break
            time.sleep(0.05)
        end = [e for e in self.recent("film") if e["data"]["phase"] == "end"]
        self.assertEqual(len(end), 1, self.recent())
        self.assertTrue(end[0]["data"]["closed"])
        self.assertNotIn(wid, self.main.wins)

    def test_film_clamps_to_the_desktop(self):
        self.make_tape()
        r = self.client.post("/show/film", json={"tape": "tiny", "x": 500, "y": -4})
        w = self.main.wins[r.json()["window_id"]]
        self.assertEqual((w["x"], w["y"]), (192 - 12, 0))

    def test_unknown_tape_is_404_with_the_list(self):
        self.make_tape()
        r = self.client.post("/show/film", json={"tape": "nope"})
        self.assertEqual(r.status_code, 404)
        self.assertIn("tiny", r.text)

    def test_tapes_route_lists_the_index(self):
        self.make_tape()
        r = self.client.get("/show/tapes")
        self.assertEqual([t["slug"] for t in r.json()["tapes"]], ["tiny"])


class NestTest(StageBase):
    def test_nest_launches_an_inner_instance_without_a_command_line(self):
        r = self.client.post("/show/nest", json={"x": 10, "y": 2, "w": 120, "h": 40})
        self.assertEqual(r.status_code, 200, r.text)
        out = r.json()
        self.assertEqual((out["instance"], out["depth"]), ("nest1", 1))
        w = self.main.wins[out["window_id"]]
        self.assertEqual(w["type"], "terminal")
        self.assertIn("WIBWOB_INSTANCE=nest1", w["exec"])
        self.assertIn("WWDOS_THEME=ega", w["exec"])
        self.assertIn("2>>", w["exec"])                       # stderr never reaches the inner screen
        self.assertEqual(w["title"], "WIBWOB-DOS (nest1)")
        ev = self.recent("nest")[0]
        self.assertEqual(ev["data"], {"phase": "open", "instance": "nest1", "depth": 1, "parent": None})

    def test_inner_verbs_and_depth(self):
        self.client.post("/show/nest", json={})
        sock1 = "/tmp/wibwob_nest1.sock"
        r = self.client.post("/show/inner/nest1/primer", json={"path": "being-wibbler.txt", "x": 1, "y": 1,
                                                               "w": 20, "h": 10, "colour": "cyan", "ink": 0})
        self.assertEqual(r.status_code, 200, r.text)
        prim = self.inner[sock1].wins[r.json()["window_id"]]
        self.assertTrue(prim["path"].endswith("modules/wibwob-2026/primers/being-wibbler.txt"))
        self.assertEqual((prim["colour"], prim["ink"]), ("cyan", "0"))
        r = self.client.post("/show/inner/nest1/figlet", json={"text": "HI", "x": 2, "y": 2, "w": 20, "h": 7})
        self.assertEqual(self.inner[sock1].wins[r.json()["window_id"]]["text"], "HI")
        r = self.client.post("/show/inner/nest1/type", json={"content": "ab\nc", "cps": 400})
        self.assertEqual(self.inner[sock1].texts[r.json()["window_id"]], "ab\nc")
        r = self.client.post("/show/inner/nest1/chrome", json={"part": "status", "text": "inside"})
        self.assertEqual(self.inner[sock1].chrome["status"], "inside")
        r = self.client.post("/show/inner/nest1/glide", json={"id": prim["id"], "x": 30, "y": 5, "ms": 100})
        self.assertEqual((prim["x"], prim["y"]), (30, 5))
        r = self.client.post("/show/inner/nest1/nest", json={})
        self.assertEqual(r.status_code, 200, r.text)
        self.assertEqual((r.json()["instance"], r.json()["depth"], r.json()["parent"]), ("nest2", 2, "nest1"))
        t2 = self.inner[sock1].wins[r.json()["window_id"]]
        self.assertIn("WIBWOB_INSTANCE=nest2", t2["exec"])       # opened inside nest1, not on the main desktop
        r = self.client.post("/show/inner/nest2/nest", json={})
        self.assertEqual(r.json()["depth"], 3)
        self.assertEqual(self.client.post("/show/inner/nest9/primer", json={"path": "x"}).status_code, 404)
        self.assertEqual(self.client.post("/show/inner/nest1/bogus", json={}).status_code, 400)

    def test_close_all_closes_every_window_deepest_first(self):
        self.client.post("/show/nest", json={})
        self.client.post("/show/inner/nest1/nest", json={})
        outer = [w for w in self.main.wins.values() if w["type"] == "terminal"][0]["id"]
        r = self.client.post("/show/nest/close_all")
        self.assertEqual(sorted(r.json()["closed"]), ["nest1", "nest2"])
        self.assertNotIn(outer, self.main.wins)
        inner_closes = [c for c in self.inner["/tmp/wibwob_nest1.sock"].calls if c[0] == "close_window"]
        self.assertEqual(len(inner_closes), 1)                    # nest2's window, closed inside nest1
        closes = [e["data"]["instance"] for e in self.recent("nest") if e["data"]["phase"] == "close"]
        self.assertEqual(closes, ["nest2", "nest1"])
        self.assertEqual(self.client.get("/show/nest").json()["nests"], [])


class ThemeAndColourTest(StageBase):
    def test_theme_variant_ega(self):
        r = self.client.post("/theme/variant", json={"variant": "ega"})
        self.assertEqual(r.status_code, 200, r.text)
        self.assertEqual(self.main.theme, "ega")
        self.assertEqual(self.client.post("/theme/variant", json={"variant": "neon"}).status_code, 422)

    def test_window_colour_on_create_and_props(self):
        r = self.client.post("/windows", json={"type": "figlet_text", "rect": {"x": 1, "y": 1, "w": 30, "h": 8},
                                               "props": {"text": "WOB", "colour": "magenta", "ink": 14}})
        self.assertEqual(r.status_code, 200, r.text)
        created = [c for c in self.main.calls if c[0] == "create_window"][-1][1]
        self.assertEqual((created["colour"], created["ink"], created["text"]), ("magenta", "14", "WOB"))
        wid = r.json()["id"]
        r = self.client.post(f"/props/{wid}", json={"props": {"colour": "cyan"}})
        self.assertEqual(r.status_code, 200, r.text)
        sent = [c[1] for c in self.main.calls if c[0] == "exec_command" and c[1]["name"] == "window_colour"][-1]
        self.assertEqual((sent["id"], sent["colour"]), (wid, "cyan"))
        r = self.client.post(f"/props/{wid}", json={"props": {"colour": "pink"}})
        self.assertEqual(r.status_code, 400)


if __name__ == "__main__":
    unittest.main()
