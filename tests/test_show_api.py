#!/usr/bin/env python3
"""Contract tests for the show layer (tools/api_server/show.py) and the paint route fix.

Runs without the C++ app: IPC is replaced by a fake desktop that keeps window rects.
    tools/api_server/venv/bin/python -m unittest tests/test_show_api.py -v
Live check against a running app instead: see README "Show endpoints" (curl lines).
"""
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
os.environ.setdefault("WIBWOB_SHOW_TMP", tempfile.mkdtemp(prefix="wwdos-show-test-"))
os.environ.pop("WIBWOB_CAPTURE_TMUX", None)

from fastapi.testclient import TestClient  # noqa: E402

import tools.api_server.controller as controller_mod  # noqa: E402
import tools.api_server.show as show_mod  # noqa: E402


class FakeDesk:
    """Minimal stand-in for the app's IPC: windows with rects, chrome text, a screen."""

    def __init__(self):
        self.wins = {}
        self.n = 0
        self.chrome = {}
        self.calls = []
        self.texts = {}

    def add(self, x, y, w, h, title="", typ="frame_player"):
        self.n += 1
        wid = f"w{self.n}"
        self.wins[wid] = dict(id=wid, title=title, type=typ, x=x, y=y, width=w, height=h)
        return wid

    def __call__(self, cmd, kv=None):
        kv = {k: str(v) for k, v in (kv or {}).items()}
        self.calls.append((cmd, kv))
        if cmd == "get_state":
            return json.dumps({"windows": list(self.wins.values())})
        if cmd == "get_canvas_size":
            return json.dumps({"width": 120, "height": 41})
        if cmd == "move_window":
            self.wins[kv["id"]].update(x=int(kv["x"]), y=int(kv["y"]))
            return '{"success":true}'
        if cmd == "move_many":
            for item in kv["moves"].split(";"):
                f = item.split(",")
                self.wins[f[0]].update(x=int(f[1]), y=int(f[2]))
                if len(f) >= 5:
                    self.wins[f[0]].update(width=int(f[3]), height=int(f[4]))
            return '{"success":true}'
        if cmd == "close_window":
            self.wins.pop(kv["id"], None)
            return "ok"
        if cmd == "send_text":
            self.texts[kv["id"]] = (self.texts.get(kv["id"], "") if kv["mode"] == "append" else "") + kv["content"]
            return "ok"
        if cmd in ("paint_line", "paint_rect"):
            return "ok" if kv["id"] in self.wins else "err paint window not found"
        if cmd == "exec_command":
            name = kv["name"]
            if name == "open_primer":
                assert Path(kv["path"]).exists(), kv["path"]
                self.add(int(kv["x"]), int(kv["y"]), int(kv["w"]), int(kv["h"]), kv.get("title", ""))
                return "ok"
            if name == "chrome_set":
                self.chrome[kv["part"]] = kv.get("text", "")
                return "ok"
            if name == "window_title":
                self.wins[kv["id"]]["title"] = kv["title"]
                return "ok"
            if name == "paint_bare":
                return "ok" if kv["id"] in self.wins else "err paint window not found"
            return "ok"
        if cmd == "screenshot":
            return "ok"
        return "ok"


class ShowApiTest(unittest.TestCase):
    def setUp(self):
        self.desk = FakeDesk()
        self._orig_show_send = show_mod.send_cmd
        self._orig_ctl_send = controller_mod.send_cmd
        show_mod.send_cmd = self.desk
        controller_mod.send_cmd = self.desk
        show_mod.capture_text = lambda: "\n".join("." * 120 for _ in range(43))
        from tools.api_server.main import make_app
        self.client = TestClient(make_app())

    def tearDown(self):
        show_mod.send_cmd = self._orig_show_send
        controller_mod.send_cmd = self._orig_ctl_send

    def recent(self, typ=None):
        evs = self.client.get("/events/recent", params={"n": 2000}).json()["events"]
        return [e for e in evs if typ is None or e["type"] == typ]

    # typed reveal ---------------------------------------------------------
    def test_send_text_with_cps_types_key_by_key(self):
        wid = self.desk.add(5, 5, 30, 8, "note", "text_editor")
        r = self.client.post(f"/windows/{wid}/send_text", json={"content": "hi wob", "cps": 200, "actor": "wib"})
        self.assertEqual(r.status_code, 200, r.text)
        self.assertEqual(self.desk.texts[wid], "hi wob")
        keys = [c for c in self.desk.calls if c[0] == "send_text"]
        self.assertEqual(len(keys), 6)                      # one IPC call per character
        evs = self.recent("type")
        self.assertEqual([e["data"]["char"] for e in evs], list("hi wob"))
        self.assertEqual(evs[0]["actor"], "wib")
        self.assertEqual(evs[0]["window"]["id"], wid)

    def test_typed_newline_travels_with_the_next_character(self):
        wid = self.desk.add(5, 5, 30, 8, "note", "text_editor")
        self.client.post(f"/windows/{wid}/send_text", json={"content": "a\nb", "cps": 300})
        sent = [c[1]["content"] for c in self.desk.calls if c[0] == "send_text"]
        self.assertEqual(sent, ["a", "\nb"])
        self.assertEqual(self.desk.texts[wid], "a\nb")

    def test_type_title_target(self):
        wid = self.desk.add(5, 5, 30, 8, "")
        r = self.client.post(f"/windows/{wid}/type", json={"content": "keep", "cps": 300, "target": "title"})
        self.assertEqual(r.status_code, 200, r.text)
        self.assertEqual(self.desk.wins[wid]["title"], "keep")

    # glide -----------------------------------------------------------------
    def test_move_with_ms_glides_through_intermediate_positions(self):
        wid = self.desk.add(0, 0, 20, 6)
        r = self.client.post(f"/windows/{wid}/move", json={"x": 40, "y": 10, "ms": 300, "actor": "wob"})
        self.assertEqual(r.status_code, 200, r.text)
        self.assertEqual((self.desk.wins[wid]["x"], self.desk.wins[wid]["y"]), (40, 10))
        steps = [c for c in self.desk.calls if c[0] == "move_many"]
        self.assertGreater(len(steps), 3)
        moves = self.recent("move")
        self.assertEqual(moves[0]["data"]["phase"], "start")
        self.assertIn("end", [m["data"].get("phase") for m in moves])

    def test_move_without_ms_is_a_jump(self):
        wid = self.desk.add(0, 0, 20, 6)
        r = self.client.post(f"/windows/{wid}/move", json={"x": 7, "y": 3})
        self.assertEqual(r.status_code, 200)
        self.assertFalse([c for c in self.desk.calls if c[0] == "move_many"])
        self.assertEqual(self.desk.wins[wid]["x"], 7)

    # cursors ---------------------------------------------------------------
    def test_cursor_events_and_sprite(self):
        self.client.post("/cursors/on", json={})
        r = self.client.post("/cursors/wib", json={"x": 30, "y": 12, "ms": 100})
        self.assertEqual(r.status_code, 200, r.text)
        body = r.json()
        self.assertEqual((body["x"], body["y"]), (30, 12))
        self.assertIn(body["window_id"], self.desk.wins)
        self.assertEqual(self.client.post("/cursors/zed", json={"x": 1, "y": 1}).status_code, 400)
        self.assertTrue(self.recent("cursor"))

    # shape -----------------------------------------------------------------
    def test_shape_opens_one_window_per_mask_cell(self):
        mask = "#.#\n###"
        r = self.client.post("/windows/shape", json={"mask": mask, "cell": "8x4", "x": 10, "y": 5,
                                                     "stagger_ms": 0, "titles": ["a", "b"]})
        self.assertEqual(r.status_code, 200, r.text)
        ids = r.json()["window_ids"]
        self.assertEqual(len(ids), 5)
        rects = sorted((self.desk.wins[i]["x"], self.desk.wins[i]["y"]) for i in ids)
        self.assertEqual(rects, [(10, 5), (10, 9), (18, 9), (26, 5), (26, 9)])
        self.assertEqual(self.desk.wins[ids[0]]["title"], "a")
        self.assertIn("scramble", self.client.get("/windows/shape/masks").json()["masks"])

    # gravity ---------------------------------------------------------------
    def test_fall_lands_every_window_on_the_floor_or_the_pile(self):
        a = self.desk.add(10, 2, 20, 6)
        b = self.desk.add(12, 12, 20, 6)
        r = self.client.post("/windows/fall", json={"jitter": 0, "fps": 60, "g": 400, "bounce": 0})
        self.assertEqual(r.status_code, 200, r.text)
        self.assertEqual(set(r.json()["landed"]), {a, b})
        rows = 40                                         # canvas 41 minus the status line
        ya, yb = self.desk.wins[a]["y"], self.desk.wins[b]["y"]
        self.assertEqual(yb + 6 - 1, rows - 1)            # b on the floor
        self.assertEqual(ya + 6, yb)                      # a on top of b
        lands = [e for e in self.recent("land") if "order" in e["data"]]
        self.assertEqual([e["data"]["order"] for e in lands], [1, 2])

    # droste ----------------------------------------------------------------
    def test_droste_nests_shrinking_windows_and_ends_with_the_cat(self):
        r = self.client.post("/windows/droste", json={"depth": 4, "pause_ms": 0})
        self.assertEqual(r.status_code, 200, r.text)
        body = r.json()
        self.assertGreaterEqual(body["levels"], 3)
        sizes = [self.desk.wins[i]["width"] for i in body["window_ids"][:body["levels"]]]
        self.assertEqual(sizes, sorted(sizes, reverse=True))
        self.assertTrue(body["bottom"])
        for prev, cur in zip(body["window_ids"], body["window_ids"][1:body["levels"]]):
            p, c = self.desk.wins[prev], self.desk.wins[cur]
            self.assertTrue(p["x"] < c["x"] and c["x"] + c["width"] <= p["x"] + p["width"])

    # chrome ----------------------------------------------------------------
    def test_chrome_sets_menu_status_and_resets(self):
        r = self.client.post("/chrome", json={"menu": "  Feel  Edit  Wonder", "status": "zz"})
        self.assertEqual(r.status_code, 200, r.text)
        self.assertEqual(self.desk.chrome["menu"], "  Feel  Edit  Wonder")
        self.client.post("/chrome", json={"reset": True})
        self.assertIn("reset", self.desk.chrome)

    # capture ---------------------------------------------------------------
    def test_capture_writes_numbered_frames_and_events(self):
        run = Path(tempfile.mkdtemp(prefix="wwdos-capture-test-"))
        self.client.post("/capture/start", json={"dir": str(run), "settle_ms": 0})
        wid = self.desk.add(0, 0, 20, 6)
        self.client.post(f"/windows/{wid}/glide", json={"x": 30, "y": 5, "ms": 200})
        self.client.post("/events/mark", json={"label": "beat-2"})
        r = self.client.post("/capture/stop").json()
        frames = sorted(run.glob("*.txt"))
        self.assertEqual(r["frames"], len(frames))
        self.assertTrue(frames[0].name.startswith("00001-start"))
        self.assertTrue(any("mark" in f.name for f in frames))
        lines = (run / "events.jsonl").read_text().strip().split("\n")
        self.assertTrue(all({"seq", "t", "type", "actor", "window", "data"} <= set(json.loads(l)) for l in lines))

    # paint fixes -------------------------------------------------------------
    def test_paint_line_and_rect_no_longer_500(self):
        wid = self.desk.add(0, 0, 40, 14, "Paint", "paint")
        for route in ("/paint/line", "/paint/rect"):
            r = self.client.post(route, json={"win_id": wid, "x0": 1, "y0": 1, "x1": 9, "y1": 5})
            self.assertEqual(r.status_code, 200, f"{route}: {r.text}")
        r = self.client.post("/paint/line", json={"win_id": "nope", "x0": 1, "y0": 1, "x1": 9, "y1": 5})
        self.assertEqual(r.status_code, 422)

    def test_paint_bare(self):
        wid = self.desk.add(0, 0, 40, 14, "Paint", "paint")
        self.assertEqual(self.client.post("/paint/bare", json={"win_id": wid}).status_code, 200)
        self.assertEqual(self.client.post("/paint/bare", json={"win_id": "nope"}).status_code, 404)


if __name__ == "__main__":
    unittest.main()
