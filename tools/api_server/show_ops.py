"""Fast control surface for a director model: batches, names, relative placement, scene snapshots,
timelines, and the nested-desktop dive.

- POST /show/batch     many ops in one call; windows are addressed by the name the director gave (`as`)
- GET  /show/scene     compact scene graph + a 64x18 minimap (the element-reference snapshot)
- POST /show/timeline  ops scheduled by the API on the wall clock (or locked to a film's epoch)
- POST /show/dive      open nested desktops one inside another, a beat apart; POST /show/surface closes them

Op shapes, place shapes and the scene format are in tools/api_server/README.md § Director endpoints.
IPC: ops that change existing windows are queued and sent as one `batch` IPC message; ops that open a
window are sent at once so their id is known to later ops in the same batch.
"""
from __future__ import annotations

import asyncio
import base64
import json
import os
import time
import unicodedata
from functools import lru_cache
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

from . import show as show_mod
from . import show_stage as stage_mod

ROOT = Path(os.environ.get("WIBWOB_REPO_ROOT", Path(__file__).resolve().parents[2]))
SHELF = ROOT / "modules" / "wibwob-2026" / "primers"
MAX_OPS = 40
COLOURS = ("blue", "cyan", "grey", "gray", "black", "magenta", "red", "green", "brown", "default")
GRID_COLS, GRID_ROWS = 6, 4
MINIMAP = (64, 18)
SEEDS = ["scramble-sitting", "face-wib-and-wob-small-pair", "being-wibbler", "being-niff-small",
         "scramble-asleep", "cat-classic-small", "being-mini-mech"]
KINDS = {"frame_player": "primer", "text_view": "text", "text_editor": "editor", "figlet_text": "figlet",
         "terminal": "terminal"}


class OpError(Exception):
    pass


# ───────────────────────────── IPC wire ─────────────────────────────

def wire(cmd: str, kv: Dict[str, Any]) -> str:
    """One IPC command line, same format as ipc_client.send_cmd (and % is escaped so text survives)."""
    parts = [f"cmd:{cmd}"]
    for k, v in kv.items():
        if v is None:
            continue
        v = str(v)
        if k == "content" and any(c in v for c in "\n\r "):
            parts.append(f"{k}=base64:" + base64.b64encode(v.encode("utf-8")).decode("ascii"))
        else:
            parts.append(f"{k}=" + v.replace("%", "%25").replace(" ", "%20").replace("\n", "%0A")
                         .replace("\r", "%0D"))
    return " ".join(parts)


def _raw(line: str, sock: Optional[str]) -> str:
    """Send one already-encoded command line to a socket and read the reply (retries a dropped socket)."""
    import socket as _socket
    from .ipc_client import get_sock_path
    path = sock or get_sock_path()
    for attempt in range(4):
        s = _socket.socket(_socket.AF_UNIX, _socket.SOCK_STREAM)
        s.settimeout(10.0)
        try:
            s.connect(path)
            s.sendall((line + "\n").encode("utf-8"))
            chunks = []
            while True:
                c = s.recv(65536)
                if not c:
                    break
                chunks.append(c)
            return b"".join(chunks).decode("utf-8", errors="ignore").strip()
        except OSError:
            if attempt == 3:
                raise
            time.sleep(0.05 * (attempt + 1))
        finally:
            s.close()
    return ""


def send_lines(lines: List[str], sock: Optional[str] = None) -> List[str]:
    """Send command lines; more than one goes as a single `batch` IPC message (one app tick)."""
    if not lines:
        return []
    if len(lines) == 1:
        return [_raw(lines[0], sock)]
    payload = base64.b64encode(("\n".join(lines) + "\n").encode("utf-8")).decode("ascii")
    resp = _raw(f"cmd:batch lines=base64:{payload}", sock)
    try:
        return list(json.loads(resp).get("results", []))
    except (json.JSONDecodeError, AttributeError):
        return [resp] * len(lines)


def is_err(resp: str) -> bool:
    r = (resp or "").strip().lower()
    return r.startswith("err") or r.startswith('{"error"')


def parse_id(resp: str) -> Optional[str]:
    try:
        return json.loads(resp).get("id")
    except (json.JSONDecodeError, AttributeError, TypeError):
        return None


# ───────────────────────────── desk model ─────────────────────────────

def cw(ch: str) -> int:
    return 2 if unicodedata.east_asian_width(ch) in ("W", "F") else 1


@lru_cache(maxsize=512)
def primer_size(path: str) -> Tuple[int, int, bool]:
    """(content cols, content rows, animated) of a primer file."""
    try:
        text = Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return (20, 5, False)
    lines = text.split("\n")
    anim = any(l.strip() == "----" for l in lines)
    frames, cur = [], []
    for l in lines:
        if l.rstrip("\r") == "----":
            frames.append(cur)
            cur = []
        else:
            cur.append(l.rstrip("\r"))
    frames.append(cur)
    w = h = 0
    for f in frames:
        while f and not f[-1].strip():
            f.pop()
        h = max(h, len(f))
        w = max([w] + [sum(cw(c) for c in l) for l in f])
    return (w, h, anim)


def resolve_primer(spec: str) -> Path:
    p = Path(spec)
    if p.is_absolute() and p.exists():
        return p
    for cand in (ROOT / spec, SHELF / spec, SHELF / f"{spec}.txt"):
        if cand.exists():
            return cand
    raise OpError(f"no primer {spec!r} (give a path, or a slug from the shelf modules/wibwob-2026/primers)")


class Desk:
    """One read of the desktop: size, windows (front first), theme. Kept up to date as ops run."""

    def __init__(self, sock: Optional[str] = None):
        self.sock = sock
        resp = send_lines([wire("get_canvas_size", {}), wire("get_state", {})], sock)
        try:
            c = json.loads(resp[0])
            self.cols, self.rows = int(c.get("width", 80)), int(c.get("height", 24)) - 1
        except (json.JSONDecodeError, IndexError, TypeError):
            self.cols, self.rows = 80, 23
        try:
            st = json.loads(resp[1])
        except (json.JSONDecodeError, IndexError, TypeError):
            st = {}
        self.theme = st.get("theme_variant", "monochrome")
        self.wins: Dict[str, Dict[str, Any]] = {}
        self.order: List[str] = []
        for w in st.get("windows", []):
            wid = w["id"]
            self.wins[wid] = {"id": wid, "title": w.get("title", ""), "type": w.get("type", ""),
                              "x": int(w.get("x", 0)), "y": int(w.get("y", 0)),
                              "w": int(w.get("w", w.get("width", 0))), "h": int(w.get("h", w.get("height", 0))),
                              "focused": bool(w.get("focused")), "path": w.get("path"),
                              "colour": w.get("colour"), "ink": w.get("ink")}
            self.order.append(wid)

    def rect(self, wid: str) -> Tuple[int, int, int, int]:
        w = self.wins[wid]
        return (w["x"], w["y"], w["w"], w["h"])

    def put(self, wid: str, x: int, y: int, w: int, h: int, **extra) -> None:
        d = self.wins.setdefault(wid, {"id": wid, "title": "", "type": ""})
        d.update(x=x, y=y, w=w, h=h, **extra)
        if wid in self.order:
            self.order.remove(wid)
        self.order.insert(0, wid)

    def drop(self, wid: str) -> None:
        self.wins.pop(wid, None)
        if wid in self.order:
            self.order.remove(wid)


# ───────────────────────────── placement ─────────────────────────────

def _overlap(a, b) -> int:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    dx = min(ax + aw, bx + bw) - max(ax, bx)
    dy = min(ay + ah, by + bh) - max(ay, by)
    return dx * dy if dx > 0 and dy > 0 else 0


def _clamp(x, y, w, h, cols, rows):
    w, h = max(1, min(w, cols)), max(1, min(h, rows))
    return max(0, min(x, cols - w)), max(0, min(y, rows - h)), w, h


def cell_rect(cell: str, cols: int, rows: int) -> Tuple[int, int, int, int]:
    """'C2' -> the rect of that cell of a 6x4 grid (columns A-F, rows 1-4); 'B2:C3' spans."""
    def one(c):
        c = c.strip().upper()
        if len(c) < 2 or not ("A" <= c[0] <= chr(ord("A") + GRID_COLS - 1)) or not c[1:].isdigit():
            raise OpError(f"cell {cell!r}: use A-F then 1-4, e.g. C2 or B2:C3")
        col, row = ord(c[0]) - ord("A"), int(c[1:]) - 1
        if not 0 <= row < GRID_ROWS:
            raise OpError(f"cell {cell!r}: rows are 1-4")
        return col, row
    a, _, b = cell.partition(":")
    c0, r0 = one(a)
    c1, r1 = one(b) if b else (c0, r0)
    c0, c1 = sorted((c0, c1))
    r0, r1 = sorted((r0, r1))
    x0, x1 = round(c0 * cols / GRID_COLS), round((c1 + 1) * cols / GRID_COLS)
    y0, y1 = round(r0 * rows / GRID_ROWS), round((r1 + 1) * rows / GRID_ROWS)
    return x0, y0, x1 - x0, y1 - y0


def _anchor_in(box, w, h, at: str, margin: int):
    bx, by, bw, bh = box
    at = (at or "center").lower().replace("_", "-")
    if at in ("center", "centre", "middle"):
        return bx + (bw - w) // 2, by + (bh - h) // 2
    v, _, hz = at.partition("-") if "-" in at else (at, "", "")
    if v in ("left", "right") and not hz:
        v, hz = "", v
    x = {"left": bx + margin, "right": bx + bw - w - margin}.get(hz, bx + (bw - w) // 2)
    y = {"top": by + margin, "bottom": by + bh - h - margin}.get(v, by + (bh - h) // 2)
    return x, y


def resolve_place(place: Dict[str, Any], w: Optional[int], h: Optional[int], desk: Desk,
                  lookup, self_id: Optional[str] = None) -> Tuple[int, int, int, int]:
    """A place spec -> (x, y, w, h) inside the desktop, nudged off other windows unless avoid=false."""
    cols, rows = desk.cols, desk.rows
    gap = int(place.get("gap", 1))
    margin = int(place.get("margin", 1 if "anchor" in place else 0))
    ref_id = None
    axis = "both"
    if "cell" in place:
        cx, cy, cwid, chei = cell_rect(str(place["cell"]), cols, rows)
        w = w or cwid
        h = h or chei
        x, y = _anchor_in((cx, cy, cwid, chei), min(w, cwid) if ":" in str(place["cell"]) else w,
                          min(h, chei) if ":" in str(place["cell"]) else h, place.get("at", "center"), 0)
        if ":" in str(place["cell"]):
            w, h = min(w, cwid), min(h, chei)
    elif "anchor" in place or place.get("center"):
        w, h = w or 30, h or 10
        x, y = _anchor_in((0, 0, cols, rows), w, h, "center" if place.get("center") else place["anchor"],
                          margin)
    else:
        rel = next((k for k in ("right_of", "left_of", "above", "below", "on", "inside") if k in place), None)
        if not rel:
            raise OpError(f"place {place!r}: give one of right_of, left_of, above, below, on, inside, cell, "
                          f"anchor, center")
        ref_id = lookup(place[rel])
        if not ref_id or ref_id not in desk.wins:
            raise OpError(f"place: no window named {place[rel]!r}")
        rx, ry, rw, rh = desk.rect(ref_id)
        align = place.get("align", "start")
        if rel == "inside":
            box = (rx + 1, ry + 1, max(1, rw - 2), max(1, rh - 2))
            w, h = min(w or box[2], box[2]), min(h or box[3], box[3])
            x, y = _anchor_in(box, w, h, place.get("at", "center"), margin)
            axis = "none"
        else:
            w, h = w or rw, h or rh
            if rel in ("right_of", "left_of"):
                x = rx + rw + gap if rel == "right_of" else rx - w - gap
                y = {"center": ry + (rh - h) // 2, "end": ry + rh - h}.get(align, ry)
                axis = "y"
            elif rel in ("above", "below"):
                y = ry - h - gap if rel == "above" else ry + rh + gap
                x = {"center": rx + (rw - w) // 2, "end": rx + rw - w}.get(align, rx)
                axis = "x"
            else:   # on: rests on the top frame of the ref
                y = ry - h
                at = place.get("at", "center")
                x = {"left": rx + 1, "right": rx + rw - w - 1}.get(at, rx + (rw - w) // 2)
                axis = "none"
    x += int(place.get("dx", 0))
    y += int(place.get("dy", 0))
    x, y, w, h = _clamp(x, y, w, h, cols, rows)
    if place.get("avoid", True) and axis != "none":
        others = [desk.rect(i) for i in desk.order if i not in (self_id, ref_id)
                  and desk.wins[i]["w"] >= 3 and desk.wins[i]["h"] >= 2]
        def cost(px, py):
            return sum(_overlap((px, py, w, h), o) for o in others)
        if others and cost(x, y):
            best = (cost(x, y), 0, x, y)
            steps = [(0, d) for d in range(1, rows)] if axis == "y" else \
                [(d, 0) for d in range(1, cols, 2)] if axis == "x" else \
                [(dx, dy) for r in range(1, 13) for dx, dy in ((2 * r, 0), (-2 * r, 0), (0, r), (0, -r),
                                                                (2 * r, r), (-2 * r, -r), (2 * r, -r), (-2 * r, r))]
            for dx, dy in steps:
                for sgn in ((1,) if axis == "both" else (1, -1)):
                    px, py, _, _ = _clamp(x + sgn * dx, y + sgn * dy, w, h, cols, rows)
                    c = cost(px, py)
                    dist = abs(px - x) + abs(py - y)
                    if (c, dist) < best[:2]:
                        best = (c, dist, px, py)
                if best[0] == 0:
                    break
            x, y = best[2], best[3]
    return x, y, w, h


# ───────────────────────────── requests ─────────────────────────────

class BatchReq(BaseModel):
    ops: List[Dict[str, Any]] = []
    atomic: bool = False
    actor: Optional[str] = None


class TimelineReq(BaseModel):
    steps: List[Dict[str, Any]]
    start_ms: int = 0
    at: Optional[float] = None          # wall-clock epoch the step times count from (e.g. a film's `at`)
    id: Optional[str] = None
    actor: Optional[str] = None


class DiveReq(BaseModel):
    depth: int = 5
    gap_ms: int = 700
    themes: Optional[List[str]] = None
    seed: Optional[List[str]] = None
    x: Optional[int] = None
    y: Optional[int] = None
    w: Optional[int] = None
    h: Optional[int] = None
    scale: float = 0.68                 # each level's window width, as a share of the desktop it opens in
    vscale: float = 0.86                # and its height (rows run out first: each level spends 4 on chrome)
    actor: Optional[str] = None


class SurfaceReq(BaseModel):
    gap_ms: int = 400
    actor: Optional[str] = None


# ───────────────────────────── the director surface ─────────────────────────────

class Director:
    def __init__(self, bus, show, stage, ctl) -> None:
        self.bus = bus
        self.show = show
        self.stage = stage
        self.ctl = ctl
        self.names: Dict[str, str] = {}             # name -> window id (main desktop)
        self.meta: Dict[str, Dict[str, Any]] = {}   # window id -> {name, kind, slug, text, colour, ink}
        self.timelines: Dict[str, Dict[str, Any]] = {}
        self._tl_n = 0
        self._tasks: set = set()
        self.snap: Dict[str, Tuple[Any, int]] = {}  # window id -> (signature, scene seq when it changed)
        self.gone: List[Tuple[int, str, str]] = []  # (scene seq, id, name)
        self.scene_seq = 0
        self.lock = asyncio.Lock()

    def _spawn(self, coro) -> None:
        t = asyncio.ensure_future(coro)
        self._tasks.add(t)
        t.add_done_callback(self._tasks.discard)

    # names --------------------------------------------------------------
    def prune(self, desk: Desk) -> None:
        for name, wid in list(self.names.items()):
            if wid not in desk.wins:
                del self.names[name]
        for wid in list(self.meta):
            if wid not in desk.wins:
                del self.meta[wid]

    def lookup(self, desk: Desk):
        def find(ref: Any) -> Optional[str]:
            if ref is None:
                return None
            ref = str(ref)
            if ref in self.names:
                return self.names[ref]
            if ref in desk.wins:
                return ref
            hits = [i for i, w in desk.wins.items() if w.get("title") == ref]
            return hits[0] if len(hits) == 1 else None
        return find

    def name_of(self, wid: str, desk: Desk) -> str:
        m = self.meta.get(wid, {})
        return m.get("name") or desk.wins.get(wid, {}).get("title") or wid

    def remember(self, wid: str, name: Optional[str], **meta) -> None:
        if name:
            for n, i in list(self.names.items()):
                if i == wid and n != name:
                    del self.names[n]
            self.names[str(name)] = wid
        m = self.meta.setdefault(wid, {})
        if name:
            m["name"] = str(name)
        m.update({k: v for k, v in meta.items() if v is not None})

    # one op ---------------------------------------------------------------
    def _size(self, op, default=(None, None)):
        return op.get("w", default[0]), op.get("h", default[1])

    def _rect_for_new(self, op, desk, lookup, w, h):
        if op.get("place"):
            return resolve_place(op["place"], w, h, desk, lookup)
        w, h = w or 30, h or 10
        if op.get("x") is None or op.get("y") is None:
            return _clamp((desk.cols - w) // 2, (desk.rows - h) // 2, w, h, desk.cols, desk.rows)
        return _clamp(int(op["x"]), int(op["y"]), w, h, desk.cols, desk.rows)

    def _target(self, op, desk, lookup, required=True) -> Optional[str]:
        ref = op.get("target", op.get("id", op.get("name")))
        if ref is None:
            if required:
                raise OpError("needs target (a name given with `as`, a title or a window id)")
            return None
        wid = lookup(ref)
        if not wid:
            raise OpError(f"no window named {ref!r}")
        return wid

    async def run_op(self, op: Dict[str, Any], ctx: Dict[str, Any]) -> Dict[str, Any]:
        """Apply one op. Ops on existing windows queue IPC lines in ctx['lines']; the rest flush first."""
        desk: Desk = ctx["desk"]
        lookup = self.lookup(desk)
        actor = op.get("actor", ctx.get("actor"))
        kind = str(op.get("op", "")).lower()
        res: Dict[str, Any] = {"op": kind}

        async def flush():
            await self.flush(ctx)

        if kind == "primer":
            path = resolve_primer(str(op.get("path") or op.get("slug") or op.get("primer") or ""))
            pw, ph, anim = primer_size(str(path))
            w0 = min(desk.cols, max(12, pw + (2 if anim else 3)))
            h0 = min(desk.rows, max(3, ph + 2))
            w, h = self._size(op, (w0, h0))
            x, y, w, h = self._rect_for_new(op, desk, lookup, w, h)
            await flush()
            kv = {"name": "open_primer", "actor": "show", "path": str(path), "x": x, "y": y, "w": w, "h": h,
                  "title": op.get("title", ""), "colour": op.get("colour"), "ink": op.get("ink"),
                  "frameless": 1 if op.get("frameless") else None,
                  "shadowless": 1 if op.get("shadowless") else None, "want_id": 1}
            r = send_lines([wire("exec_command", kv)])[0]
            if is_err(r):
                raise OpError(r)
            wid = parse_id(r)
            desk.put(wid, x, y, w, h, title=op.get("title", ""), type="frame_player", path=str(path),
                     colour=op.get("colour"), ink=op.get("ink"))
            self.remember(wid, op.get("as"), kind="primer", slug=path.stem, size=f"{pw}x{ph}",
                          colour=op.get("colour"), ink=op.get("ink"))
            res.update(id=wid, rect=[x, y, w, h])
            await self.bus.emit("open", actor=actor, window={"id": wid, "title": op.get("title", ""), "x": x,
                                "y": y, "w": w, "h": h}, op="primer", name=op.get("as"), slug=path.stem)
        elif kind == "figlet":
            text = str(op.get("text", "WIBWOB"))
            w0 = min(desk.cols, max(16, len(text) * 7 + 4))
            w, h = self._size(op, (w0, 9))
            x, y, w, h = self._rect_for_new(op, desk, lookup, w, h)
            await flush()
            kv = {"type": "figlet_text", "text": text, "font": op.get("font", "standard"), "x": x, "y": y,
                  "w": w, "h": h, "title": op.get("title"), "colour": op.get("colour"), "ink": op.get("ink"),
                  "frameless": 1 if op.get("frameless") else None}
            r = send_lines([wire("create_window", kv)])[0]
            if is_err(r) or not parse_id(r):
                raise OpError(r or "figlet window did not open")
            wid = parse_id(r)
            desk.put(wid, x, y, w, h, title=op.get("title", ""), type="figlet_text")
            self.remember(wid, op.get("as"), kind="figlet", text=text, colour=op.get("colour"), ink=op.get("ink"))
            res.update(id=wid, rect=[x, y, w, h])
            await self.bus.emit("figlet", actor=actor, window={"id": wid, "title": op.get("title", ""), "x": x,
                                "y": y, "w": w, "h": h}, op="figlet", name=op.get("as"), text=text)
        elif kind in ("text", "type"):
            content = str(op.get("content", op.get("text", "")))
            wid = self._target(op, desk, lookup, required=False)
            if not wid:
                w, h = self._size(op, (40, 10))
                x, y, w, h = self._rect_for_new(op, desk, lookup, w, h)
                await flush()
                r = send_lines([wire("create_window", {"type": "text_editor", "x": x, "y": y, "w": w, "h": h,
                                                       "title": op.get("title") or op.get("as") or "",
                                                       "colour": op.get("colour"),
                                                       "ink": op.get("ink")})])[0]
                wid = parse_id(r)
                if not wid:
                    raise OpError(r or "editor did not open")
                desk.put(wid, x, y, w, h, title=op.get("title", ""), type="text_editor")
                self.remember(wid, op.get("as"), kind="editor", colour=op.get("colour"), ink=op.get("ink"))
                res.update(rect=[x, y, w, h])
                await self.bus.emit("open", actor=actor, window={"id": wid, "title": op.get("title", ""),
                                    "x": x, "y": y, "w": w, "h": h}, op="text", name=op.get("as"))
            cps = op.get("cps", 14 if kind == "type" else None)
            self.remember(wid, None, text=(self.meta.get(wid, {}).get("text", "") + content)[-80:])
            res.update(id=wid, chars=len(content))
            if content and cps:
                await flush()
                req = show_mod.TypeReq(content=content, cps=float(cps), mode=op.get("mode", "append"),
                                       target=op.get("target_kind", "editor"), actor=actor)
                task = self.show.type_text(wid, req)
                if op.get("wait"):
                    await task
                else:
                    self._spawn(task)
                    res["ms"] = int(len(content) / float(cps) * 1000)
            elif content:
                self.q(ctx, wire("send_text", {"id": wid, "content": content,
                                                       "mode": op.get("mode", "append"), "position": "end"}))
                ctx["events"].append(("type", actor, {"id": wid}, {"op": kind, "chars": len(content)}))
        elif kind in ("move", "glide"):
            wid = self._target(op, desk, lookup)
            x0, y0, w0, h0 = desk.rect(wid)
            w, h = op.get("w", w0), op.get("h", h0)
            if op.get("place"):
                x, y, w, h = resolve_place(op["place"], w, h, desk, lookup, self_id=wid)
            elif "dx" in op or "dy" in op:
                x, y, w, h = _clamp(x0 + int(op.get("dx", 0)), y0 + int(op.get("dy", 0)), w, h, desk.cols, desk.rows)
            elif op.get("x") is not None and op.get("y") is not None:
                x, y = int(op["x"]), int(op["y"])
            else:
                raise OpError(f"{kind} needs x, y or place or dx/dy")
            res.update(id=wid, rect=[x, y, w, h])
            if kind == "glide" or op.get("ms"):
                await flush()
                req = show_mod.GlideReq(x=x, y=y, w=w, h=h, ms=int(op.get("ms", 600)),
                                        ease=op.get("ease", "inout"), actor=actor)
                desk.put(wid, x, y, w, h)
                if op.get("wait"):
                    await self.show.glide(wid, req)
                else:
                    self._spawn(self.show.glide(wid, req))
                res["ms"] = req.ms
            else:
                spec = f"{wid},{x},{y}" + (f",{w},{h}" if (w, h) != (w0, h0) else "")
                self.q(ctx, wire("move_many", {"moves": spec}))
                desk.put(wid, x, y, w, h)
                ctx["events"].append(("move", actor, {"id": wid, "title": desk.wins[wid].get("title"), "x": x,
                                      "y": y, "w": w, "h": h}, {"phase": "end", "op": "move"}))
        elif kind in ("colour", "color", "props"):
            wid = self._target(op, desk, lookup)
            colour = op.get("colour", op.get("color"))
            if colour is not None and str(colour) not in COLOURS:
                raise OpError(f"colour must be one of {', '.join(COLOURS)}")
            ink = op.get("ink")
            if ink is not None and not (0 <= int(ink) <= 15):
                raise OpError("ink must be 0-15")
            self.q(ctx, wire("exec_command", {"name": "window_colour", "actor": "show", "id": wid,
                                                      "colour": colour or "", "ink": "" if ink is None else ink}))
            self.remember(wid, None, colour=colour, ink=ink)
            res.update(id=wid)
            ctx["events"].append(("colour", actor, {"id": wid}, {"op": "colour", "colour": colour, "ink": ink}))
        elif kind == "front":
            wid = self._target(op, desk, lookup)
            self.q(ctx, wire("focus_window", {"id": wid}))
            desk.put(wid, *desk.rect(wid))
            res.update(id=wid)
            ctx["events"].append(("front", actor, {"id": wid}, {"op": "front"}))
        elif kind == "close":
            wid = self._target(op, desk, lookup)
            self.q(ctx, wire("close_window", {"id": wid}))
            name = self.name_of(wid, desk)
            desk.drop(wid)
            self.names = {n: i for n, i in self.names.items() if i != wid}
            res.update(id=wid)
            ctx["events"].append(("close", actor, {"id": wid, "title": name}, {"op": "close"}))
        elif kind == "title":
            wid = self._target(op, desk, lookup)
            text = str(op.get("text", op.get("title", "")))
            self.q(ctx, wire("exec_command", {"name": "window_title", "actor": "show", "id": wid,
                                                      "title": text}))
            desk.wins[wid]["title"] = text
            res.update(id=wid)
            ctx["events"].append(("chrome", actor, {"id": wid}, {"part": "title", "text": text, "op": "title"}))
        elif kind == "chrome":
            if op.get("cps"):
                await flush()
                req = show_mod.ChromeReq(menu=op.get("menu"), status=op.get("status"), kaomoji=op.get("kaomoji"),
                                         reset=bool(op.get("reset")), cps=float(op["cps"]), actor=actor)
                self._spawn(self.show.chrome(req))
            elif op.get("reset"):
                self.q(ctx, wire("exec_command", {"name": "chrome_set", "actor": "show", "part": "reset"}))
                ctx["events"].append(("chrome", actor, None, {"part": "reset", "op": "chrome"}))
            else:
                for part in ("menu", "status", "kaomoji"):
                    if op.get(part) is not None:
                        self.q(ctx, wire("exec_command", {"name": "chrome_set", "actor": "show",
                                                                  "part": part, "text": op[part] or " "}))
                        ctx["events"].append(("chrome", actor, None, {"part": part, "text": op[part], "op": "chrome"}))
        elif kind == "theme":
            variant = str(op.get("variant", "ega"))
            if variant not in ("ega", "monochrome", "dark_pastel"):
                raise OpError("variant must be ega, monochrome or dark_pastel")
            self.q(ctx, wire("exec_command", {"name": "set_theme_variant", "actor": "show",
                                                      "variant": variant, "sweep_ms": op.get("sweep_ms")}))
            ctx["events"].append(("chrome", actor, None, {"part": "theme", "text": variant, "op": "theme",
                                                          "sweep_ms": op.get("sweep_ms", 0)}))
            res.update(variant=variant)
        elif kind == "wait":
            await flush()
            await asyncio.sleep(max(0, min(10000, int(op.get("ms", 0)))) / 1000.0)
        elif kind == "cursor":
            wid = self._target(op, desk, lookup, required=False)
            if wid:
                x, y, w, h = desk.rect(wid)
                cx, cy = x + w // 2, y
            else:
                cx, cy = int(op.get("x", 0)), int(op.get("y", 0))
            a = op.get("actor") or actor or "wib"
            if a not in show_mod.ACTORS:
                raise OpError(f"cursor actor must be one of {show_mod.ACTORS}")
            await flush()
            self._spawn(self.show.cursor_to(a, cx, cy, int(op.get("ms", 350))))
            res.update(x=cx, y=cy)
        elif kind == "film":
            tape = str(op.get("tape", ""))
            idx = self.stage.tape_index()
            if tape not in idx:
                raise OpError(f"no tape {tape!r}; have: {', '.join(sorted(idx))}")
            fw, fh = int(idx[tape]["cols"]) + 2, int(idx[tape]["rows"]) + 2
            x = op.get("x")
            y = op.get("y")
            if op.get("place"):
                x, y, _, _ = resolve_place(op["place"], fw, fh, desk, lookup)
            await flush()
            out = await self.stage.film(stage_mod.FilmReq(tape=tape, x=x, y=y, title=op.get("title"), actor=actor,
                                                          lead_ms=int(op.get("lead_ms", 1200)),
                                                          close_after=bool(op.get("close_after"))))
            wid = out["window_id"]
            desk.put(wid, out["x"], out["y"], out["w"], out["h"], title=out["title"], type="terminal")
            self.remember(wid, op.get("as"), kind="film", slug=tape, text=out["title"])
            res.update(id=wid, rect=[out["x"], out["y"], out["w"], out["h"]], at=out["at"],
                       duration=out["duration"], audio=out["audio"])
        elif kind == "nest":
            w, h = self._size(op)
            if op.get("place"):
                x, y, w, h = resolve_place(op["place"], w or int(desk.cols * 0.62), h or int(desk.rows * 0.78),
                                           desk, lookup)
            else:
                x, y = op.get("x"), op.get("y")
            await flush()
            out = await self.stage.nest(stage_mod.NestReq(x=x, y=y, w=w, h=h, title=op.get("title"),
                                                          theme=op.get("theme", "ega"), actor=actor))
            wid = out["window_id"]
            desk.put(wid, out["x"], out["y"], out["w"], out["h"], type="terminal")
            self.remember(wid, op.get("as"), kind="nest", slug=out["instance"], text=out["instance"])
            res.update(id=wid, instance=out["instance"], depth=out["depth"],
                       rect=[out["x"], out["y"], out["w"], out["h"]])
        elif kind == "inner":
            inst = str(op.get("instance", ""))
            if inst in self.names and self.meta.get(self.names[inst], {}).get("kind") == "nest":
                inst = self.meta[self.names[inst]]["slug"]
            verb = str(op.get("verb", ""))
            fields = {k: v for k, v in op.items() if k not in ("op", "instance", "verb")}
            await flush()
            out = await self.stage.inner(inst, verb, stage_mod.InnerReq(**fields))
            res.update(out)
        else:
            raise OpError(f"unknown op {kind!r} (primer, figlet, text, type, move, glide, colour, front, close, "
                          f"title, chrome, theme, wait, cursor, film, nest, inner)")
        if op.get("as") and res.get("id") and kind not in ("primer", "figlet", "text", "type", "film", "nest"):
            self.remember(res["id"], op["as"])
        if op.get("as"):
            res["as"] = op["as"]
        return res

    def q(self, ctx: Dict[str, Any], line: str) -> None:
        ctx["lines"].append((line, ctx.get("cur", -1)))

    async def flush(self, ctx: Dict[str, Any]) -> None:
        """Send the queued lines as one IPC message, mark refused ops, then emit their events."""
        lines, events = ctx["lines"], ctx["events"]
        ctx["lines"], ctx["events"] = [], []
        if lines:
            resps = send_lines([l for l, _ in lines])
            for r, (_, i) in zip(resps, lines):
                if is_err(r) and 0 <= i < len(ctx["results"]):
                    ctx["results"][i].update(ok=False, error=r.strip())
        for typ, actor, win, data in events:
            await self.bus.emit(typ, actor=actor, window=win, **data)

    # batch -----------------------------------------------------------------
    def validate(self, ops: List[Dict[str, Any]], desk: Desk) -> List[Optional[str]]:
        """Cheap checks before anything runs (for atomic batches): op names, targets, place refs."""
        lookup = self.lookup(desk)
        known = set()
        errs: List[Optional[str]] = []
        need_target = {"move", "glide", "colour", "color", "props", "front", "close", "title"}
        for op in ops:
            kind = str(op.get("op", "")).lower()
            err = None
            ref = op.get("target", op.get("id", op.get("name")))
            if kind not in need_target | {"primer", "figlet", "text", "type", "chrome", "theme", "wait", "cursor",
                                          "film", "nest", "inner"}:
                err = f"unknown op {kind!r}"
            elif kind in need_target and (ref is None or (str(ref) not in known and not lookup(ref))):
                err = f"no window named {ref!r}"
            if not err and op.get("place"):
                p = op["place"]
                for rel in ("right_of", "left_of", "above", "below", "on", "inside"):
                    if rel in p and str(p[rel]) not in known and not lookup(p[rel]):
                        err = f"place: no window named {p[rel]!r}"
                if "cell" in p:
                    try:
                        cell_rect(str(p["cell"]), desk.cols, desk.rows)
                    except OpError as e:
                        err = str(e)
            if not err and kind == "primer":
                try:
                    resolve_primer(str(op.get("path") or op.get("slug") or op.get("primer") or ""))
                except OpError as e:
                    err = str(e)
            if kind in ("colour", "color", "props") and op.get("colour") is not None \
                    and str(op.get("colour")) not in COLOURS:
                err = "unknown colour"
            if op.get("as"):
                known.add(str(op["as"]))
            errs.append(err)
        return errs

    async def batch(self, ops: List[Dict[str, Any]], atomic: bool = False, actor: Optional[str] = None,
                    sync: bool = True) -> Dict[str, Any]:
        if len(ops) > MAX_OPS:
            raise HTTPException(413, f"at most {MAX_OPS} ops per batch")
        t0 = time.perf_counter()
        async with self.lock:
            desk = Desk()
            self.prune(desk)
            if atomic:
                errs = self.validate(ops, desk)
                if any(errs):
                    return {"ok": False, "applied": 0, "atomic": True,
                            "results": [{"i": i, "op": op.get("op"), "ok": e is None, **({"error": e} if e else {})}
                                        for i, (op, e) in enumerate(zip(ops, errs))]}
            ctx = {"desk": desk, "lines": [], "events": [], "results": [], "actor": actor}
            changed = False
            for i, op in enumerate(ops):
                res = {"i": i, "op": op.get("op"), "ok": True}
                ctx["results"].append(res)
                ctx["cur"] = i
                try:
                    res.update(await self.run_op(op, ctx))
                    changed = True
                except OpError as e:
                    res.update(ok=False, error=str(e))
                except HTTPException as e:
                    res.update(ok=False, error=str(e.detail))
                except Exception as e:   # one op must not sink the rest
                    res.update(ok=False, error=f"{type(e).__name__}: {e}")
            await self.flush(ctx)
            if changed and sync:
                await self.ctl._sync_state()
                live = {w.id: w for w in self.ctl._state.windows}
                last = {r.get("id"): r["i"] for r in ctx["results"] if r.get("id")}
                for res in ctx["results"]:
                    w = live.get(res.get("id"))
                    # the app has the final word on a rect (it can clamp); only the last op on a window
                    if w is not None and last.get(res.get("id")) == res["i"] and "rect" in res \
                            and res.get("op") not in ("close", "glide") and not res.get("ms"):
                        res["rect"] = [w.rect.x, w.rect.y, w.rect.w, w.rect.h]
        applied = sum(1 for r in ctx["results"] if r.get("ok"))
        return {"ok": applied == len(ops), "applied": applied, "ms": round((time.perf_counter() - t0) * 1000, 1),
                "results": ctx["results"]}

    # scene -------------------------------------------------------------------
    def scene(self, since: Optional[int] = None, minimap: bool = True, full: bool = False) -> Dict[str, Any]:
        desk = Desk()
        self.prune(desk)
        live = set(desk.wins)
        bump = False
        sigs = {}
        for z, wid in enumerate(desk.order):
            w = desk.wins[wid]
            m = self.meta.get(wid, {})
            sig = (w["x"], w["y"], w["w"], w["h"], w.get("title"), w.get("colour"), w.get("ink"), z,
                   m.get("name") or w.get("title") or wid, m.get("text"))
            sigs[wid] = sig
        changed = [wid for wid in desk.order if self.snap.get(wid, (None,))[0] != sigs[wid]]
        removed = [wid for wid in self.snap if wid not in live]
        if changed or removed:
            self.scene_seq = max(self.scene_seq + 1, self.bus.seq)
            for wid in changed:
                self.snap[wid] = (sigs[wid], self.scene_seq)
            for wid in removed:
                self.gone.append((self.scene_seq, wid, self.snap[wid][0][8]))
                del self.snap[wid]
            self.gone = self.gone[-200:]
        focus = next((i for i in desk.order if desk.wins[i].get("focused")), None)
        rows_out, items = [], []
        for z, wid in enumerate(desk.order):
            if since is not None and self.snap.get(wid, (None, 0))[1] <= since:
                continue
            w = desk.wins[wid]
            m = self.meta.get(wid, {})
            name = m.get("name") or w.get("title") or wid
            kind = m.get("kind") or KINDS.get(w.get("type", ""), w.get("type", ""))
            slug = m.get("slug") or (Path(w["path"]).stem if w.get("path") else "")
            colour = w.get("colour") or m.get("colour") or ""
            ink = w.get("ink") if w.get("ink") is not None else m.get("ink")
            col = f"{colour}/{ink}" if ink is not None and str(ink) != "" else colour
            info = m.get("text") or m.get("size") or ""
            if not info and w.get("path"):
                pw, ph, _ = primer_size(w["path"])
                info = f"{pw}x{ph}"
            info = " ".join(str(info).split())[:40]
            rows_out.append("|".join([name if name == wid else f"{name}", kind, slug,
                                      f"{w['x']},{w['y']},{w['w']},{w['h']}", col, str(z),
                                      info.replace("|", "/")]) + ("" if name == wid else f"|{wid}"))
            if full:
                items.append({"name": name, "id": wid, "kind": kind, "slug": slug,
                              "rect": [w["x"], w["y"], w["w"], w["h"]], "colour": colour, "ink": ink, "z": z,
                              "focused": wid == focus, "text": info})
        out: Dict[str, Any] = {"seq": self.scene_seq, "desk": [desk.cols, desk.rows], "theme": desk.theme,
                               "focus": self.name_of(focus, desk) if focus else None,
                               "cols": "name|kind|slug|x,y,w,h|colour/ink|z|text|id", "windows": rows_out}
        if since is not None:
            out["since"] = since
            out["gone"] = [n for s, _, n in self.gone if s > since]
        nests = [n["instance"] for n in self.stage.nests.values()]
        if nests:
            out["nests"] = nests
        if minimap and (since is None or rows_out or out.get("gone")):
            out["minimap"] = self.minimap(desk)
        if full:
            out["items"] = items
        return out

    def minimap(self, desk: Desk) -> str:
        mw, mh = MINIMAP
        grid = [[" "] * mw for _ in range(mh)]
        sx, sy = mw / max(1, desk.cols), mh / max(1, desk.rows)
        for wid in reversed(desk.order):           # back to front
            w = desk.wins[wid]
            x0, y0 = int(w["x"] * sx), int(w["y"] * sy)
            x1 = max(x0, min(mw - 1, int((w["x"] + w["w"] - 1) * sx)))
            y1 = max(y0, min(mh - 1, int((w["y"] + w["h"] - 1) * sy)))
            for yy in range(y0, y1 + 1):
                for xx in range(x0, x1 + 1):
                    edge_y, edge_x = yy in (y0, y1), xx in (x0, x1)
                    grid[yy][xx] = "+" if edge_y and edge_x else "-" if edge_y else "|" if edge_x else " "
            name = self.name_of(wid, desk)
            letter = (name[:1] or "?")
            if x1 - x0 >= 2:
                grid[y0][x0 + 1] = letter
            else:
                grid[y0][x0] = letter
        return "\n".join("".join(r).rstrip() for r in grid)

    # timeline ------------------------------------------------------------------
    async def timeline(self, req: TimelineReq) -> Dict[str, Any]:
        if not req.steps:
            raise HTTPException(400, "steps is empty")
        if len(req.steps) > 400:
            raise HTTPException(413, "at most 400 steps")
        for s in req.steps:
            if "op" not in s or not isinstance(s["op"], dict):
                raise HTTPException(400, "each step is {\"t\": ms, \"op\": {...}}")
        self._tl_n += 1
        tid = req.id or f"tl{self._tl_n}"
        base = req.at if req.at is not None else time.time()
        base += req.start_ms / 1000.0
        steps = sorted(req.steps, key=lambda s: float(s.get("t", 0)))
        rec = {"id": tid, "starts_at": round(base, 3), "ends_at": round(base + float(steps[-1].get("t", 0)) / 1000, 3),
               "steps": len(steps), "applied": 0, "failed": 0, "state": "scheduled", "errors": []}
        self.timelines[tid] = rec

        async def run():
            await self.bus.emit("timeline", actor=req.actor, id=tid, phase="start", steps=len(steps),
                                starts_at=rec["starts_at"])
            i = 0
            try:
                while i < len(steps):
                    t = float(steps[i].get("t", 0))
                    group = []
                    while i < len(steps) and float(steps[i].get("t", 0)) == t:
                        group.append(steps[i]["op"])
                        i += 1
                    delay = base + t / 1000.0 - time.time()
                    if delay > 0:
                        await asyncio.sleep(delay)
                    rec["state"] = "running"
                    out = await self.batch(group, actor=req.actor, sync=False)
                    for r in out["results"]:
                        if r.get("ok"):
                            rec["applied"] += 1
                        else:
                            rec["failed"] += 1
                            rec["errors"].append({"t": t, "op": r.get("op"), "error": r.get("error")})
                rec["state"] = "done"
                await self.ctl._sync_state()
                await self.bus.emit("timeline", actor=req.actor, id=tid, phase="end", applied=rec["applied"],
                                    failed=rec["failed"])
            except asyncio.CancelledError:
                rec["state"] = "cancelled"
                await self.bus.emit("timeline", actor=req.actor, id=tid, phase="cancel", applied=rec["applied"])
                raise

        task = asyncio.ensure_future(run())
        rec["_task"] = task
        return {k: v for k, v in rec.items() if not k.startswith("_")}

    def cancel(self, tid: str) -> Optional[Dict[str, Any]]:
        rec = self.timelines.get(tid)
        if not rec:
            return None
        task = rec.get("_task")
        if task and not task.done():
            task.cancel()
            rec["state"] = "cancelled"
        return {k: v for k, v in rec.items() if not k.startswith("_")}

    # dive / surface --------------------------------------------------------------
    async def dive(self, req: DiveReq) -> Dict[str, Any]:
        t0 = time.time()
        desk = Desk()
        depth = max(1, min(stage_mod.NEST_MAX_DEPTH, req.depth))
        if req.themes:
            themes = [t if t in ("ega", "monochrome", "dark_pastel") else "ega" for t in req.themes]
        else:
            first = "monochrome" if desk.theme == "ega" else "ega"
            other = "ega" if first == "monochrome" else "monochrome"
            themes = [first if k % 2 == 0 else other for k in range(depth)]
        seeds = []
        for s in (req.seed or SEEDS):
            try:
                seeds.append(resolve_primer(s))
            except OpError:
                pass
        levels = []
        parent = None
        stopped = None

        xfac: Dict[int, float] = {}

        def child_rect(cols, rows, k):
            w = req.w if (k == 0 and req.w) else max(22, int(cols * req.scale))
            h = req.h if (k == 0 and req.h) else max(6, int(rows * req.vscale))
            # vanishing point right of centre and low: the left strip stays free for the seed
            x = req.x if (k == 0 and req.x is not None) else int((cols - w) * xfac.get(k, 0.66))
            y = req.y if (k == 0 and req.y is not None) else int((rows - h) * 0.72)
            return x, y, w, h

        for k in range(depth):
            theme = themes[k % len(themes)]
            cols, rows = (desk.cols, desk.rows) if parent is None else stage_mod.desk_at(parent["sock"])
            x, y, w, h = child_rect(cols, rows, k)
            if w > cols or h > rows or w < 22 or h < 6:
                stopped = f"stopped before depth {k + 1}: the desktop at depth {k} is {cols}x{rows}"
                break
            title = f"WIBWOB-DOS {k + 1}"
            if parent is None:
                out = await self.stage.nest(stage_mod.NestReq(x=x, y=y, w=w, h=h, title=title, theme=theme,
                                                              actor=req.actor))
            else:
                out = await self.stage.inner(parent["instance"], "nest", stage_mod.InnerReq(
                    x=x, y=y, w=w, h=h, title=title, theme=theme, actor=req.actor))
            inst = out["instance"]
            rec = self.stage.nests[inst]
            seed = None
            icols, irows = w - 2, h - 4            # the inner desktop: frame, menu bar, status line
            if seeds and irows >= 2:
                sp = seeds[k % len(seeds)]
                pw, ph, anim = primer_size(str(sp))
                want_w, want_h = pw + (2 if anim else 3), ph + 2
                if k == depth - 1:                 # deepest level: the whole desktop is free; centre it
                    sw, sh = min(want_w, icols), min(want_h, irows)
                    sx, sy = max(0, (icols - sw) // 2), max(0, (irows - sh) // 2)
                else:                              # left of where the next level will open
                    nx, ny, nw, nh = child_rect(icols, irows, k + 1)
                    if nx - 1 < min(want_w, 10):   # too narrow: open the next level flush right
                        xfac[k + 1] = 1.0
                        nx, ny, nw, nh = child_rect(icols, irows, k + 1)
                    if nx - 1 >= min(want_w, 8):
                        sw, sh, sx, sy = min(want_w, nx - 1), min(want_h, irows), 0, 0
                    elif ny >= 3:
                        sw, sh, sx, sy = min(want_w, icols), min(want_h, ny), 0, 0
                    else:
                        sw = sh = 0
                if sw >= 6 and sh >= 3:
                    try:
                        r = await self.stage.inner(inst, "primer", stage_mod.InnerReq(
                            path=str(sp), x=sx, y=sy, w=sw, h=sh, actor=req.actor,
                            title=sp.stem.replace("being-", "", 1).split("-")[0]))
                        seed = {"slug": sp.stem, "rect": [sx, sy, sw, sh], "window_id": r.get("window_id")}
                    except HTTPException:
                        seed = None
            levels.append({"instance": inst, "depth": rec["depth"], "theme": theme, "window_id": out["window_id"],
                           "parent": rec["parent"], "rect": [x, y, w, h], "desktop": [icols, irows],
                           "seed": seed, "t": round(time.time() - t0, 3)})
            parent = rec
            if k < depth - 1:
                await asyncio.sleep(max(0, req.gap_ms) / 1000.0)
        await self.bus.emit("dive", actor=req.actor, phase="end", depth=len(levels),
                            instances=[l["instance"] for l in levels])
        out = {"levels": levels, "depth": len(levels), "ms": round((time.time() - t0) * 1000)}
        if stopped:
            out["stopped"] = stopped
        return out

    async def surface(self, req: SurfaceReq) -> Dict[str, Any]:
        t0 = time.time()
        self.stage.adopt()
        order = sorted(self.stage.nests.values(), key=lambda r: -r["depth"])
        closed = []
        for i, rec in enumerate(order):
            await self.stage._close_one(rec["instance"])
            closed.append({"instance": rec["instance"], "depth": rec["depth"], "t": round(time.time() - t0, 3)})
            if i < len(order) - 1:
                await asyncio.sleep(max(0, req.gap_ms) / 1000.0)
        await self.bus.emit("dive", actor=req.actor, phase="surface", closed=[c["instance"] for c in closed])
        return {"closed": closed, "ms": round((time.time() - t0) * 1000)}


# ───────────────────────────── routes ─────────────────────────────

def register(app: FastAPI, ctl) -> Director:
    d = Director(app.state.show_bus, app.state.show, app.state.stage, ctl)
    app.state.director = d

    @app.post("/show/batch", summary="Many ops in one call; windows addressed by name; per-op results")
    async def batch_route(req: BatchReq) -> Dict[str, Any]:
        return await d.batch(req.ops, atomic=req.atomic, actor=req.actor)

    @app.get("/show/scene", summary="Compact scene graph + minimap (element references for a director)")
    async def scene_route(since: Optional[int] = None, minimap: bool = True, full: bool = False) -> Dict[str, Any]:
        return d.scene(since=since, minimap=minimap, full=full)

    @app.post("/show/place", summary="Resolve a place spec to a rect without moving anything")
    async def place_route(body: Dict[str, Any]) -> Dict[str, Any]:
        desk = Desk()
        d.prune(desk)
        try:
            x, y, w, h = resolve_place(body.get("place") or {}, body.get("w"), body.get("h"), desk, d.lookup(desk),
                                       self_id=d.lookup(desk)(body.get("target")))
        except OpError as e:
            raise HTTPException(400, str(e))
        return {"rect": [x, y, w, h]}

    @app.post("/show/timeline", summary="Schedule ops on the wall clock (or a film's epoch); returns an id")
    async def timeline_route(req: TimelineReq) -> Dict[str, Any]:
        return await d.timeline(req)

    @app.get("/show/timeline/{tid}")
    async def timeline_get(tid: str) -> Dict[str, Any]:
        rec = d.timelines.get(tid)
        if not rec:
            raise HTTPException(404, f"no timeline {tid}")
        return {k: v for k, v in rec.items() if not k.startswith("_")}

    @app.post("/show/dive", summary="Open nested desktops one inside another, a beat apart")
    async def dive_route(req: DiveReq) -> Dict[str, Any]:
        return await d.dive(req)

    @app.post("/show/surface", summary="Close the nested desktops deepest first, a beat apart")
    async def surface_route(req: Optional[SurfaceReq] = None) -> Dict[str, Any]:
        return await d.surface(req or SurfaceReq())

    return d
