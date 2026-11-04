#!/usr/bin/env python3
"""Contract tests for the director surface (tools/api_server/show_ops.py): batch, names, place, scene,
timeline, dive/surface, and the theme sweep route. No C++ app: a fake desktop answers the IPC lines,
including the `batch` message.
    tools/api_server/venv/bin/python -m unittest tests/test_show_ops.py -v
"""
import base64
import json
import os
import sys
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tests"))

from test_show_stage import FakeApp, StageBase  # noqa: E402

import tools.api_server.show_ops as ops_mod  # noqa: E402


def parse_line(line):
    head, _, rest = line.partition(" ")
    kv = {}
    for tok in rest.split(" "):
        if "=" in tok:
            k, v = tok.split("=", 1)
            if v.startswith("base64:"):
                v = base64.b64decode(v[7:]).decode()
            else:
                v = v.replace("%0A", "\n").replace("%0D", "\r").replace("%20", " ").replace("%25", "%")
            kv[k] = v
    return head[4:], kv


class OpsFakeApp(FakeApp):
    def __call__(self, cmd, kv=None):
        kv = {k: str(v) for k, v in (kv or {}).items()}
        if cmd == "exec_command" and kv.get("name") == "open_primer" and kv.get("want_id") == "1":
            r = super().__call__(cmd, kv)
            return json.dumps({"ok": True, "id": f"w{self.n}"}) if r == "ok" else r
        if cmd == "focus_window":
            self.calls.append((cmd, kv))
            w = self.wins.pop(kv["id"])
            self.wins[kv["id"]] = w                       # last in the dict = front (state lists it first)
            return '{"success":true}'
        if cmd == "move_many":
            self.calls.append((cmd, kv))
            for item in kv["moves"].split(";"):
                f = item.split(",")
                self.wins[f[0]].update(x=int(f[1]), y=int(f[2]))
                if len(f) >= 5:
                    self.wins[f[0]].update(width=int(f[3]), height=int(f[4]))
            return '{"success":true}'
        if cmd == "get_state":
            self.calls.append((cmd, kv))
            ws = list(self.wins.values())[::-1]          # front first, like the app
            return json.dumps({"theme_variant": self.theme, "windows": ws})
        return super().__call__(cmd, kv)


class OpsBase(StageBase):
    def setUp(self):
        super().setUp()
        self.main = OpsFakeApp("main")
        import tools.api_server.show as show_mod
        import tools.api_server.controller as controller_mod
        show_mod.send_cmd = self.main
        controller_mod.send_cmd = self.main
        self.raw_calls = []
        self._orig_raw = ops_mod._raw
        ops_mod._raw = self.fake_raw

    def tearDown(self):
        ops_mod._raw = self._orig_raw
        super().tearDown()

    def fake_raw(self, line, sock=None):
        app = self.main if sock is None else self.inner[sock]
        cmd, kv = parse_line(line)
        self.raw_calls.append((sock, cmd, kv))
        if cmd == "batch":
            sub = [l for l in kv["lines"].split("\n") if l]
            return json.dumps({"results": [app(*parse_line(l)) for l in sub]})
        return app(cmd, kv)

    def batch(self, ops, **kw):
        r = self.client.post("/show/batch", json={"ops": ops, **kw})
        self.assertEqual(r.status_code, 200, r.text)
        return r.json()

    def rect(self, wid):
        w = self.main.wins[wid]
        return [w["x"], w["y"], w["width"], w["height"]]


class BatchTest(OpsBase):
    def test_names_places_and_one_ipc_message_for_queued_ops(self):
        out = self.batch([
            {"op": "primer", "slug": "being-wibbler", "as": "wib", "place": {"cell": "A1"}},
            {"op": "primer", "slug": "scramble-sitting", "as": "cat", "place": {"right_of": "wib", "gap": 2}},
            {"op": "move", "target": "cat", "place": {"below": "wib"}},
            {"op": "colour", "target": "wib", "colour": "magenta", "ink": 14},
            {"op": "title", "target": "cat", "text": "scramble"},
            {"op": "front", "target": "wib"},
            {"op": "figlet", "text": "HI", "as": "sign", "place": {"anchor": "top-right"}},
            {"op": "close", "target": "sign"},
        ])
        self.assertTrue(out["ok"], out)
        res = {r["i"]: r for r in out["results"]}
        wib, cat = res[0]["id"], res[1]["id"]
        wx, wy, ww, wh = self.rect(wib)
        self.assertEqual((wx, wy), ((192 // 6 - ww) // 2, (52 // 4 - wh) // 2))   # centred in cell A1
        self.assertEqual(res[1]["rect"][0], wx + ww + 2)                          # right_of wib, gap 2
        self.assertEqual(self.rect(cat)[1], wy + wh + 1)                          # moved below wib
        self.assertEqual(self.main.wins[wib]["colour"], "magenta")
        self.assertEqual(self.main.wins[cat]["title"], "scramble")
        self.assertNotIn(res[6]["id"], self.main.wins)
        batches = [k for s, c, k in self.raw_calls if c == "batch"]
        moved = [b for b in batches if "move_many" in b["lines"] and "window_colour" in b["lines"]
                 and "window_title" in b["lines"] and "focus_window" in b["lines"]]
        self.assertEqual(len(moved), 1, "the queued ops travel as one IPC message")
        types = [e["type"] for e in self.recent()]
        for t in ("open", "move", "colour", "chrome", "front", "figlet", "close"):
            self.assertIn(t, types)

    def test_refusals_per_op_and_atomic(self):
        out = self.batch([
            {"op": "primer", "slug": "being-wibbler", "as": "wib"},
            {"op": "move", "target": "nobody", "x": 1, "y": 1},
            {"op": "colour", "target": "wib", "colour": "pink"},
            {"op": "dance"},
        ])
        self.assertFalse(out["ok"])
        self.assertEqual(out["applied"], 1)
        errs = {r["i"]: r.get("error", "") for r in out["results"]}
        self.assertIn("no window named 'nobody'", errs[1])
        self.assertIn("colour", errs[2])
        self.assertIn("unknown op", errs[3])
        n = len(self.main.wins)
        out = self.batch([{"op": "primer", "slug": "being-wibbler", "as": "w2"},
                          {"op": "move", "target": "ghost", "x": 1, "y": 1}], atomic=True)
        self.assertEqual((out["ok"], out["applied"]), (False, 0))
        self.assertEqual(len(self.main.wins), n, "atomic: nothing applied")

    def test_place_route_resolves_without_moving(self):
        self.batch([{"op": "primer", "slug": "being-wibbler", "as": "wib", "x": 10, "y": 5, "w": 20, "h": 8}])
        r = self.client.post("/show/place", json={"place": {"left_of": "wib", "gap": 3}, "w": 6, "h": 4}).json()
        self.assertEqual(r["rect"], [1, 5, 6, 4])

    def test_batch_limit(self):
        r = self.client.post("/show/batch", json={"ops": [{"op": "wait", "ms": 0}] * 41})
        self.assertEqual(r.status_code, 413)


class PlaceTest(unittest.TestCase):
    def desk(self, wins):
        d = ops_mod.Desk.__new__(ops_mod.Desk)
        d.cols, d.rows, d.theme, d.sock = 120, 40, "ega", None
        d.wins, d.order = {}, []
        for wid, (x, y, w, h) in wins.items():
            d.wins[wid] = {"id": wid, "title": wid, "x": x, "y": y, "w": w, "h": h}
            d.order.append(wid)
        return d

    def place(self, d, p, w=None, h=None, self_id=None):
        return list(ops_mod.resolve_place(p, w, h, d, lambda r: r if r in d.wins else None, self_id))

    def test_relations(self):
        d = self.desk({"tower": (40, 10, 20, 12)})
        self.assertEqual(self.place(d, {"right_of": "tower", "gap": 2}, 10, 5), [62, 10, 10, 5])
        self.assertEqual(self.place(d, {"left_of": "tower"}, 10, 5), [29, 10, 10, 5])
        self.assertEqual(self.place(d, {"above": "tower", "align": "center"}, 10, 5), [45, 4, 10, 5])
        self.assertEqual(self.place(d, {"below": "tower"}, 10, 5), [40, 23, 10, 5])
        self.assertEqual(self.place(d, {"on": "tower"}, 10, 4), [45, 6, 10, 4])
        self.assertEqual(self.place(d, {"inside": "tower", "at": "top"}, 8, 3), [46, 11, 8, 3])

    def test_grid_anchor_center_and_clamp(self):
        d = self.desk({})
        self.assertEqual(self.place(d, {"cell": "C2"}, 10, 4), [40 + 5, 10 + 3, 10, 4])
        self.assertEqual(self.place(d, {"cell": "B2:C3"}), [20, 10, 40, 20])
        self.assertEqual(self.place(d, {"anchor": "bottom-left"}, 10, 4), [1, 35, 10, 4])
        self.assertEqual(self.place(d, {"center": True}, 20, 10), [50, 15, 20, 10])
        self.assertEqual(self.place(d, {"cell": "F4", "dx": 50}, 10, 4), [110, 33, 10, 4])
        with self.assertRaises(ops_mod.OpError):
            self.place(d, {"cell": "Z9"}, 5, 5)

    def test_avoids_overlap_along_the_free_axis(self):
        d = self.desk({"tower": (10, 5, 20, 10), "crate": (32, 5, 12, 6)})
        x, y, w, h = self.place(d, {"right_of": "tower", "gap": 2}, 12, 5)
        self.assertEqual(x, 32)                            # stays right of the tower
        self.assertEqual(ops_mod._overlap((x, y, w, h), (32, 5, 12, 6)), 0)   # slid clear of the crate
        self.assertEqual(y, 0)                             # the nearest free spot (up 5, not down 6)
        x, y, w, h = self.place(d, {"right_of": "tower", "gap": 2, "avoid": False}, 12, 5)
        self.assertEqual((x, y), (32, 5))


class SceneTest(OpsBase):
    def test_compact_scene_under_2k_with_20_windows_and_minimap(self):
        ops = [{"op": "primer", "slug": "being-wibbler", "as": f"win{i}", "place": {"cell": f"{'ABCDEF'[i % 6]}{i // 6 + 1}"}}
               for i in range(20)]
        self.assertTrue(self.batch(ops)["ok"])
        r = self.client.get("/show/scene")
        self.assertLess(len(r.text), 2000, len(r.text))
        d = r.json()
        self.assertEqual(len(d["windows"]), 20)
        self.assertTrue(d["windows"][0].split("|")[0].startswith("win"))
        mm = d["minimap"].split("\n")
        self.assertEqual(len(mm), 18)
        self.assertLessEqual(max(len(l) for l in mm), 64)
        self.assertIn("w", d["minimap"])
        self.assertEqual(d["cols"], "name|kind|slug|x,y,w,h|colour/ink|z|text|id")

    def test_since_returns_only_changes(self):
        self.batch([{"op": "primer", "slug": "being-wibbler", "as": "a", "x": 1, "y": 1},
                    {"op": "primer", "slug": "being-wibbler", "as": "b", "x": 40, "y": 1}])
        seq = self.client.get("/show/scene").json()["seq"]
        self.batch([{"op": "move", "target": "b", "dx": 3}, {"op": "close", "target": "a"}])
        d = self.client.get("/show/scene", params={"since": seq}).json()
        self.assertEqual([w.split("|")[0] for w in d["windows"]], ["b"])
        self.assertEqual(d["gone"], ["a"])
        d2 = self.client.get("/show/scene", params={"since": d["seq"]}).json()
        self.assertEqual((d2["windows"], d2["gone"]), ([], []))
        self.assertNotIn("minimap", d2)


class TimelineTest(OpsBase):
    def test_timeline_runs_steps_in_order_and_cancels(self):
        self.batch([{"op": "primer", "slug": "being-wibbler", "as": "wib", "x": 1, "y": 1}])
        wid = list(self.main.wins)[-1]
        t0 = time.time()
        r = self.client.post("/show/timeline", json={"steps": [
            {"t": 0, "op": {"op": "move", "target": "wib", "x": 10, "y": 2}},
            {"t": 100, "op": {"op": "move", "target": "wib", "x": 20, "y": 3}},
            {"t": 200, "op": {"op": "move", "target": "wib", "x": 30, "y": 4}}]}).json()
        self.assertEqual(r["steps"], 3)
        self.assertAlmostEqual(r["ends_at"] - r["starts_at"], 0.2, delta=0.01)
        for _ in range(40):
            if self.client.get(f"/show/timeline/{r['id']}").json()["state"] == "done":
                break
            time.sleep(0.05)
        st = self.client.get("/timeline/status", params={"group_id": r["id"]}).json()
        self.assertEqual((st["state"], st["applied"]), ("done", 3))
        self.assertEqual(self.rect(wid)[:2], [30, 4])
        self.assertGreaterEqual(time.time() - t0, 0.2)
        r2 = self.client.post("/show/timeline", json={"start_ms": 2000, "steps": [
            {"t": 0, "op": {"op": "move", "target": "wib", "x": 90, "y": 9}}]}).json()
        c = self.client.post("/timeline/cancel", json={"timeline_id": r2["id"]}).json()
        self.assertEqual(c["state"], "cancelled")
        time.sleep(0.1)
        self.assertEqual(self.rect(wid)[:2], [30, 4])


class DiveTest(OpsBase):
    def test_dive_alternates_themes_seeds_each_level_and_surface_closes_deepest_first(self):
        self.main.theme = "ega"
        r = self.client.post("/show/dive", json={"depth": 3, "gap_ms": 0})
        self.assertEqual(r.status_code, 200, r.text)
        d = r.json()
        self.assertEqual([l["depth"] for l in d["levels"]], [1, 2, 3])
        self.assertEqual([l["theme"] for l in d["levels"]], ["monochrome", "ega", "monochrome"])
        self.assertTrue(all(l["seed"] for l in d["levels"]), d["levels"])
        opens = [e["data"]["instance"] for e in self.recent("nest") if e["data"]["phase"] == "open"]
        self.assertEqual(opens, ["nest1", "nest2", "nest3"])
        outer = [w for w in self.main.wins.values() if w["type"] == "terminal"]
        self.assertIn("WWDOS_THEME=monochrome", outer[0]["exec"])
        s = self.client.post("/show/surface", json={"gap_ms": 0}).json()
        self.assertEqual([c["instance"] for c in s["closed"]], ["nest3", "nest2", "nest1"])

    def test_theme_sweep_reaches_the_app(self):
        r = self.client.post("/theme/variant", json={"variant": "ega", "sweep_ms": 600})
        self.assertEqual(r.status_code, 200, r.text)
        sent = [k for c, k in self.main.calls if c == "exec_command" and k["name"] == "set_theme_variant"][-1]
        self.assertEqual((sent["variant"], sent["sweep_ms"]), ("ega", "600"))


if __name__ == "__main__":
    unittest.main()
