#!/usr/bin/env python3
"""Latency bench for the WIBWOB-DOS control API: what one director action costs, end to end.

    python3 tools/api_server/bench.py --api http://127.0.0.1:8095 [--n 20] [--json out.json]

Prints p50 / p95 / mean milliseconds per row (stdlib only, run against your own instance; it opens and
closes windows):
  primer open     POST /menu/command open_primer (x, y, w, h)
  move            POST /windows/{id}/move (a jump)
  glide start     POST /windows/{id}/glide ms=200, minus the 200 ms of motion (the overhead of starting one)
  figlet          POST /windows type=figlet_text
  colour change   POST /props/{id} {colour}
  close           POST /windows/{id}/close
  batch of 10     POST /show/batch with 10 moves when it exists, else 10 sequential /move calls
  scene           GET /show/scene when it exists, else GET /state + tmux capture-pane (what a director read before)
When /show/batch exists it also times the same actions as single batch ops (op: primer, move, colour,
figlet, close), which is the path a director uses.
"""
from __future__ import annotations

import argparse
import json
import os
import statistics
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRIMER = ROOT / "modules" / "wibwob-2026" / "primers" / "being-wibbler.txt"


class Api:
    def __init__(self, base: str):
        self.base = base.rstrip("/")

    def call(self, method: str, path: str, body=None, timeout=30):
        data = None if body is None else json.dumps(body).encode()
        req = urllib.request.Request(self.base + path, data=data, method=method,
                                     headers={"content-type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=timeout) as r:
                raw = r.read()
                return r.status, (json.loads(raw) if raw else None), len(raw)
        except urllib.error.HTTPError as e:
            return e.code, e.read().decode(errors="replace"), 0

    def post(self, path, body=None):
        return self.call("POST", path, body if body is not None else {})

    def get(self, path):
        return self.call("GET", path)


def timed(fn):
    t = time.perf_counter()
    out = fn()
    return (time.perf_counter() - t) * 1000.0, out


def windows(api):
    return {w["id"]: w for w in api.get("/state")[1]["windows"]}


def open_primer(api, x=4, y=4, w=30, h=12, title="bench"):
    before = set(windows(api))
    api.post("/menu/command", {"command": "open_primer",
                               "args": {"path": str(PRIMER), "x": x, "y": y, "w": w, "h": h, "title": title}})
    new = [i for i in windows(api) if i not in before]
    return new[-1] if new else None


def stats(xs):
    xs = sorted(xs)
    if not xs:
        return {"n": 0}
    p95 = xs[min(len(xs) - 1, int(round(0.95 * (len(xs) - 1))))]
    return {"n": len(xs), "p50": round(statistics.median(xs), 1), "p95": round(p95, 1),
            "mean": round(statistics.mean(xs), 1)}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--api", default="http://127.0.0.1:8095")
    ap.add_argument("--n", type=int, default=20)
    ap.add_argument("--json")
    ap.add_argument("--tmux", default=os.environ.get("WIBWOB_CAPTURE_TMUX", "dosbuild"),
                    help="tmux session for the old-style screen read")
    a = ap.parse_args()
    api = Api(a.api)
    api.post("/windows/close_all")
    rows = {k: [] for k in ("primer open", "move", "glide start", "figlet", "colour change", "close",
                            "batch of 10", "scene")}
    have_batch = api.post("/show/batch", {"ops": []})[0] == 200
    have_scene = api.get("/show/scene")[0] == 200
    sizes = {}
    for i in range(a.n):
        # primer open: the call alone (the id lookup afterwards is not counted)
        before = set(windows(api))
        ms, _ = timed(lambda: api.post("/menu/command", {"command": "open_primer", "args": {
            "path": str(PRIMER), "x": 4 + i % 10, "y": 4, "w": 30, "h": 12, "title": f"b{i}"}}))
        rows["primer open"].append(ms)
        wid = [w for w in windows(api) if w not in before][-1]
        ms, _ = timed(lambda: api.post(f"/windows/{wid}/move", {"x": 40 + i % 20, "y": 10}))
        rows["move"].append(ms)
        ms, _ = timed(lambda: api.post(f"/windows/{wid}/glide", {"x": 60, "y": 14, "ms": 200}))
        rows["glide start"].append(max(0.0, ms - 200.0))
        ms, _ = timed(lambda: api.post(f"/props/{wid}", {"props": {"colour": "cyan" if i % 2 else "blue"}}))
        rows["colour change"].append(ms)
        ms, r = timed(lambda: api.post("/windows", {"type": "figlet_text", "rect": {"x": 90, "y": 2, "w": 40,
                                                    "h": 8}, "props": {"text": "WOB", "font": "small"}}))
        rows["figlet"].append(ms)
        if have_batch:
            ops = [{"op": "move", "id": wid, "x": 10 + k * 3, "y": 6 + k} for k in range(10)]
            ms, _ = timed(lambda: api.post("/show/batch", {"ops": ops}))
        else:
            def ten():
                for k in range(10):
                    api.post(f"/windows/{wid}/move", {"x": 10 + k * 3, "y": 6 + k})
            ms, _ = timed(ten)
        rows["batch of 10"].append(ms)
        if have_scene:
            ms, out = timed(lambda: api.get("/show/scene"))
            sizes["scene"] = out[2]
        else:
            def old_read():
                st = api.get("/state")
                cap = subprocess.run(["tmux", "capture-pane", "-p", "-t", a.tmux], capture_output=True, text=True)
                return st[2] + len(cap.stdout)
            ms, n = timed(old_read)
            sizes["scene"] = n
        rows["scene"].append(ms)
        if have_batch:
            def op(o):
                return api.post("/show/batch", {"ops": [o]})
            ms, _ = timed(lambda: op({"op": "primer", "slug": "being-wibbler", "as": f"bp{i}",
                                       "place": {"cell": "A1"}}))
            rows.setdefault("batch op: primer", []).append(ms)
            ms, _ = timed(lambda: op({"op": "move", "target": f"bp{i}", "place": {"cell": "C3"}}))
            rows.setdefault("batch op: move (place)", []).append(ms)
            ms, _ = timed(lambda: op({"op": "colour", "target": f"bp{i}", "colour": "cyan"}))
            rows.setdefault("batch op: colour", []).append(ms)
            ms, _ = timed(lambda: op({"op": "figlet", "text": "WOB", "font": "small", "as": f"bf{i}",
                                       "place": {"anchor": "top-right"}}))
            rows.setdefault("batch op: figlet", []).append(ms)
            ms, _ = timed(lambda: op({"op": "close", "target": f"bf{i}"}))
            rows.setdefault("batch op: close", []).append(ms)
        for w in list(windows(api)):
            ms, _ = timed(lambda: api.post(f"/windows/{w}/close"))
            rows["close"].append(ms)
    out = {k: stats(v) for k, v in rows.items()}
    label = {"batch of 10": "batch of 10" + ("" if have_batch else " (10 x /move)"),
             "scene": "scene" + ("" if have_scene else " (/state + capture)")}
    print(f"{'action':34s} {'p50':>8s} {'p95':>8s} {'mean':>8s}   n")
    for k, s in out.items():
        print(f"{label.get(k, k):34s} {s['p50']:8.1f} {s['p95']:8.1f} {s['mean']:8.1f}  {s['n']:2d}")
    print(f"scene payload: {sizes.get('scene', 0)} bytes")
    if a.json:
        Path(a.json).write_text(json.dumps({"rows": out, "batch": have_batch, "scene": have_scene,
                                            "scene_bytes": sizes.get("scene")}, indent=2))
    api.post("/windows/close_all")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
