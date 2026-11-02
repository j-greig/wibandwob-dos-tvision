"""Show layer for live desktop performances (GLITCH talk duet).

Adds, on top of the existing API and without changing it:
- a show event bus: every action becomes one JSON event (open, move, land, close,
  type, figlet, cursor, droste, shape, fall, chrome, capture), streamed at
  GET /events (SSE) and kept in a ring for GET /events/recent;
- a capture timeline: while a capture is running, each action writes a numbered
  .txt screen frame plus events.jsonl into a run dir (render with tools/frames_to_png.py);
- typed reveal, glide, two cursors, droste, shape layout, gravity, chrome text.

Event schema (one JSON object per event; see README "Show events"):
  {"seq": int, "t": float seconds since API start, "type": str, "actor": str|null,
   "window": {"id","title","x","y","w","h"}|null, "data": {...}}
"""
from __future__ import annotations

import asyncio
import contextlib
import io
import json
import math
import os
import random
import subprocess
import time
import unicodedata
from collections import deque
from pathlib import Path
from typing import Any, Dict, List, Optional

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import StreamingResponse
from pydantic import BaseModel

from .gridphysics import GridPhysics
from .ipc_client import send_cmd

ACTORS = ("wib", "wob", "scramble")
CURSOR_GLYPHS = {"wib": "つ◕", "wob": "┼○", "scramble": "ᐠᐟ"}
# Characters that box or vanish inside windows (see AGENTS/handover notes).
BAD_CHARS = set("~ꞈᆽᵕᴥᴗᵔᴖ⚙⁓⚬⟁⬡⬢")
TMP = Path(os.environ.get("WIBWOB_SHOW_TMP", f"/tmp/wwdos-show-{os.getpid()}"))


# ───────────────────────────── IPC helpers ─────────────────────────────

def ipc(cmd: str, kv: Optional[Dict[str, Any]] = None) -> str:
    """send_cmd without its per-call log lines (glides make hundreds of calls).
    Retries a dropped socket (BrokenPipe/reset seen under fast typing) up to 3 times."""
    for attempt in range(4):
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                resp = send_cmd(cmd, {k: str(v) for k, v in (kv or {}).items()})
            return resp or ""
        except OSError:
            if attempt == 3:
                raise
            time.sleep(0.05 * (attempt + 1))
    return ""


def exec_cmd(name: str, **args: Any) -> str:
    kv = {"name": name, "actor": "show"}
    kv.update({k: str(v) for k, v in args.items()})
    return ipc("exec_command", kv)


def windows() -> Dict[str, Dict[str, Any]]:
    try:
        st = json.loads(ipc("get_state") or "{}")
    except json.JSONDecodeError:
        return {}
    out = {}
    for w in st.get("windows", []):
        out[w["id"]] = {
            "id": w["id"], "title": w.get("title", ""), "type": w.get("type", ""),
            "x": int(w.get("x", 0)), "y": int(w.get("y", 0)),
            "w": int(w.get("width", w.get("w", 0))), "h": int(w.get("height", w.get("h", 0))),
        }
    return out


def desk() -> tuple:
    """(cols, rows) of the desktop area. get_canvas_size counts the status line."""
    try:
        c = json.loads(ipc("get_canvas_size") or "{}")
        return int(c.get("width", 80)), int(c.get("height", 24)) - 1
    except json.JSONDecodeError:
        return 80, 23


def cw(ch: str) -> int:
    return 2 if unicodedata.east_asian_width(ch) in ("W", "F") else 1


def clean_line(s: str) -> str:
    return "".join(" " if c in BAD_CHARS else c for c in s)


def write_tmp(name: str, lines: List[str]) -> Path:
    TMP.mkdir(parents=True, exist_ok=True)
    p = TMP / name
    # A line of exactly ---- would split the file into animation frames.
    safe = ["--- " if l.strip() == "----" else l for l in lines]
    p.write_text("\n".join(safe) + "\n", encoding="utf-8")
    return p


def open_text_window(lines: List[str], x: int, y: int, w: int, h: int, title: str = "",
                     frameless: bool = False, shadowless: bool = False, name: str = "") -> Optional[str]:
    """Open a frame_player window showing `lines`; return its id."""
    before = set(windows())
    stem = name or f"win-{time.time_ns()}"
    path = write_tmp(f"{stem}.txt", [clean_line(l) + " " for l in lines] or [" "])
    args = dict(path=str(path), x=x, y=y, w=w, h=h)
    if title:
        args["title"] = title
    if frameless:
        args["frameless"] = 1
    if shadowless:
        args["shadowless"] = 1
    exec_cmd("open_primer", **args)
    new = [i for i in windows() if i not in before]
    return new[-1] if new else None


def screen_cells(text: str, cols: int) -> List[List[str]]:
    """Screen text -> grid of cells; a wide char fills its cell and leaves '' in the next."""
    grid = []
    for line in text.split("\n"):
        row: List[str] = []
        for ch in line:
            row.append(ch)
            if cw(ch) == 2:
                row.append("")
        row = (row + [" "] * cols)[:cols]
        grid.append(row)
    return grid


def downsample(grid: List[List[str]], w: int, h: int) -> List[str]:
    """Nearest-cell sample of a cell grid into w x h, keeping wide chars whole."""
    sh = len(grid)
    sw = len(grid[0]) if grid else 0
    out = []
    for r in range(h):
        src = grid[min(sh - 1, int(r * sh / h))] if sh else []
        line, c = [], 0
        while c < w:
            ch = src[min(sw - 1, int(c * sw / w))] if sw else " "
            if ch == "":
                ch = " "
            if cw(ch) == 2:
                if c + 1 >= w:
                    ch = " "
                else:
                    line.append(ch)
                    c += 2
                    continue
            line.append(ch)
            c += 1
        out.append("".join(line))
    return out


# ───────────────────────────── bus + capture ─────────────────────────────

LEGACY_MAP = {
    "window.created": "open", "window.updated": "move", "window.closed": "close",
    "text.sent": "type", "figlet.sent": "figlet", "layout.tile": "layout",
    "layout.cascade": "layout",
}
COMMAND_MAP = {"open_primer": "open", "close_window": "close", "move_window": "move",
               "window_title": "chrome", "chrome_set": "chrome", "open_figlet_text": "open",
               "open_text_editor": "open"}
FRAME_TYPES = {"open", "move", "land", "close", "type", "figlet", "droste", "shape",
               "fall", "chrome", "layout", "cursor", "mark"}


class Recorder:
    def __init__(self, run_dir: Path, settle_ms: int, every: Dict[str, int]):
        self.run_dir = run_dir
        self.settle = settle_ms / 1000.0
        self.every = every
        self.n = 0
        self.counts: Dict[str, int] = {}
        run_dir.mkdir(parents=True, exist_ok=True)
        self.events = open(run_dir / "events.jsonl", "a", encoding="utf-8")
        self.frames = open(run_dir / "frames.jsonl", "a", encoding="utf-8")
        self.t0 = time.time()
        self.mark = ""

    def want(self, ev: Dict[str, Any]) -> bool:
        typ = ev["type"]
        if typ not in FRAME_TYPES:
            return False
        phase = (ev.get("data") or {}).get("phase")
        key = f"{typ}.{phase}" if phase == "step" else typ
        k = self.every.get(key, 1)
        self.counts[key] = self.counts.get(key, 0) + 1
        return k > 0 and (self.counts[key] - 1) % k == 0

    async def frame(self, label: str) -> Path:
        if self.settle:
            await asyncio.sleep(self.settle)
        text = capture_text()
        self.n += 1
        p = self.run_dir / f"{self.n:05d}-{label}.txt"
        p.write_text(text, encoding="utf-8")
        self.frames.write(json.dumps({"frame": p.name, "t": round(time.time() - self.t0, 3),
                                      "label": label, "beat": self.mark}) + "\n")
        self.frames.flush()
        return p


def capture_text() -> str:
    """The screen as text: tmux capture-pane when the app runs in a known tmux
    session (WIBWOB_CAPTURE_TMUX), else the app's own screenshot file."""
    sess = os.environ.get("WIBWOB_CAPTURE_TMUX")
    if sess:
        r = subprocess.run(["tmux", "capture-pane", "-p", "-t", sess], capture_output=True, text=True)
        if r.returncode == 0:
            return r.stdout.rstrip("\n")
    start = time.time()
    ipc("screenshot")
    root = Path(os.environ.get("WIBWOB_REPO_ROOT", Path(__file__).resolve().parents[2]))
    for _ in range(30):
        files = sorted((root / "logs" / "screenshots").glob("tui_*.txt"), key=lambda p: p.stat().st_mtime)
        if files and files[-1].stat().st_mtime >= start - 1:
            lines = files[-1].read_text(encoding="utf-8", errors="replace").split("\n")
            return "\n".join(l for l in lines if not l.startswith("#"))
        time.sleep(0.05)
    return ""


class ShowBus:
    def __init__(self) -> None:
        self.t0 = time.time()
        self.seq = 0
        self.ring: deque = deque(maxlen=2000)
        self.subs: set = set()
        self.rec: Optional[Recorder] = None

    async def emit(self, typ: str, actor: Optional[str] = None, window: Optional[Dict[str, Any]] = None,
                   capture: bool = True, **data: Any) -> Dict[str, Any]:
        self.seq += 1
        win = None
        if window:
            win = {k: window.get(k) for k in ("id", "title", "x", "y", "w", "h")}
        ev = {"seq": self.seq, "t": round(time.time() - self.t0, 3), "type": typ,
              "actor": actor, "window": win, "data": data}
        self.ring.append(ev)
        for q in list(self.subs):
            if q.qsize() < 5000:
                q.put_nowait(ev)
        if self.rec:
            if typ == "mark":
                self.rec.mark = str(data.get("label", ""))
            self.rec.events.write(json.dumps(ev, ensure_ascii=False) + "\n")
            self.rec.events.flush()
            if capture and self.rec.want(ev):
                p = await self.rec.frame(f"{typ}" + (f"-{actor}" if actor else ""))
                ev["frame"] = p.name
        return ev

    async def legacy(self, event: str, data: Dict[str, Any]) -> None:
        """Mirror the controller's events onto the show bus."""
        typ = LEGACY_MAP.get(event)
        if event == "command.executed":
            typ = COMMAND_MAP.get(data.get("command", ""))
            if not typ or not data.get("ok"):
                return
            if data.get("actor") == "show":
                return
            await self.emit(typ, actor=data.get("actor"), command=data.get("command"), source="api")
            return
        if not typ:
            return
        win = None
        if isinstance(data, dict) and "rect" in data:
            r = data["rect"]
            win = {"id": data.get("id"), "title": data.get("title"), "x": r.get("x"), "y": r.get("y"),
                   "w": r.get("w"), "h": r.get("h")}
        elif isinstance(data, dict) and ("id" in data or "window_id" in data):
            win = {"id": data.get("id") or data.get("window_id")}
        extra = {k: v for k, v in (data or {}).items() if k in ("text", "font", "mode", "cols")}
        if "content" in (data or {}):
            extra["chars"] = len(data["content"])
        await self.emit(typ, window=win, source="api", **extra)


# ───────────────────────────── request models ─────────────────────────────

class TypeReq(BaseModel):
    content: str
    cps: float = 14.0
    mode: str = "append"          # editor: append | replace (first keystroke only)
    position: str = "end"
    target: str = "editor"        # editor | title | figlet
    jitter: float = 0.3           # 0..1 random spread on each key interval
    actor: Optional[str] = None


class GlideReq(BaseModel):
    x: int
    y: int
    w: Optional[int] = None
    h: Optional[int] = None
    ms: int = 800
    ease: str = "inout"           # linear | inout | out | back | bounce
    fps: int = 24
    actor: Optional[str] = None


class CursorReq(BaseModel):
    x: int
    y: int
    ms: int = 350
    ease: str = "out"
    glyph: Optional[str] = None


class DrosteReq(BaseModel):
    depth: int = 6
    scale: float = 0.55
    x: Optional[int] = None
    y: Optional[int] = None
    w: Optional[int] = None
    h: Optional[int] = None
    title: str = ""
    pause_ms: int = 600
    last: str = "   z\n/ᐠ -w- ᐟ\\"  # what sits at the bottom (Scramble asleep); empty = nothing
    actor: Optional[str] = None


class ShapeReq(BaseModel):
    mask: str                      # mask text (non-space = window) or a built-in name
    cell: Optional[str] = None     # "WxH" outer size of each window; default fits the desktop
    x: Optional[int] = None
    y: Optional[int] = None
    fill: str = ""                 # char to fill each window with ("" = empty, "mask" = the mask char)
    primers: List[str] = []        # cycle these files through the windows instead of fill
    titles: List[str] = []         # one title per window, in open order (letters of a word)
    order: str = "rows"            # rows | random | spiral | cols
    stagger_ms: int = 60
    frameless: bool = False
    actor: Optional[str] = None


class FallReq(BaseModel):
    ids: List[str] = []            # empty = every window except cursors
    g: float = 70.0                # cells / s^2
    bounce: float = 0.3
    repose: Optional[int] = 2
    jitter: float = 5.0            # random sideways push, cells / s
    fps: int = 24
    max_s: float = 8.0
    seed: int = 7
    actor: Optional[str] = None


class ChromeReq(BaseModel):
    menu: Optional[str] = None
    status: Optional[str] = None
    kaomoji: Optional[str] = None
    reset: bool = False
    cps: float = 0.0               # >0 types the text in key by key
    actor: Optional[str] = None


class CaptureStart(BaseModel):
    dir: Optional[str] = None
    name: str = "run"
    settle_ms: int = 40
    every: Dict[str, int] = {"move.step": 3, "type": 3, "fall.step": 2, "cursor": 2}


# ───────────────────────────── built-in masks ─────────────────────────────

MASKS = {
    "scramble": [
        "##.............##",
        "###...........###",
        "#################",
        "#################",
        "###..#######..###",
        "###..#######..###",
        "#################",
        "#######.#.#######",
        "########.########",
        ".###############.",
        "..#############..",
    ],
    "tower": [
        ".#.#.#.",
        ".#####.",
        "..###..",
        "..#.#..",
        "..###..",
        "..###..",
        "#######",
    ],
    "twins": [
        "##...##",
        "#.#.#.#",
        "##...##",
        ".#...#.",
        "###.###",
    ],
    "heart": [
        ".##.##.",
        "#######",
        "#######",
        ".#####.",
        "..###..",
        "...#...",
    ],
}


def ease(kind: str, u: float) -> float:
    u = max(0.0, min(1.0, u))
    if kind == "linear":
        return u
    if kind == "out":
        return 1 - (1 - u) ** 3
    if kind == "back":
        c = 1.70158
        return 1 + (c + 1) * (u - 1) ** 3 + c * (u - 1) ** 2
    if kind == "bounce":
        n, d = 7.5625, 2.75
        if u < 1 / d:
            return n * u * u
        if u < 2 / d:
            u -= 1.5 / d
            return n * u * u + 0.75
        if u < 2.5 / d:
            u -= 2.25 / d
            return n * u * u + 0.9375
        u -= 2.625 / d
        return n * u * u + 0.984375
    return 4 * u ** 3 if u < 0.5 else 1 - (-2 * u + 2) ** 3 / 2


# ───────────────────────────── the show ─────────────────────────────

class Show:
    def __init__(self, bus: ShowBus) -> None:
        self.bus = bus
        self.cursors: Dict[str, Dict[str, Any]] = {}   # actor -> {id, x, y, glyph}
        self.cursors_on = False

    # two hands ------------------------------------------------------------
    def cursor_ids(self) -> set:
        return {c["id"] for c in self.cursors.values() if c.get("id")}

    async def cursor_to(self, actor: str, x: int, y: int, ms: int = 300, ease_kind: str = "out",
                        glyph: Optional[str] = None) -> Dict[str, Any]:
        cur = self.cursors.setdefault(actor, {"id": None, "x": x, "y": y, "glyph": CURSOR_GLYPHS.get(actor, "+")})
        if glyph:
            cur["glyph"] = glyph
        x0, y0 = cur["x"], cur["y"]
        if self.cursors_on:
            live = windows()
            if not cur["id"] or cur["id"] not in live:
                width = sum(cw(c) for c in cur["glyph"])
                cur["id"] = open_text_window([cur["glyph"]], x0, y0, width + 1, 1, frameless=True,
                                             shadowless=True, name=f"cursor-{actor}")
        steps = max(1, int(ms / 1000 * 24))
        t_start = time.time()
        for i in range(1, steps + 1):
            u = ease(ease_kind, i / steps)
            cx, cy = round(x0 + (x - x0) * u), round(y0 + (y - y0) * u)
            if (cx, cy) != (cur["x"], cur["y"]):
                cur["x"], cur["y"] = cx, cy
                if self.cursors_on and cur["id"]:
                    ipc("move_window", {"id": cur["id"], "x": cx, "y": cy})
                if i < steps:
                    await self.bus.emit("cursor", actor=actor, x=cx, y=cy, phase="step")
            delay = t_start + i * ms / 1000 / steps - time.time()
            if delay > 0:
                await asyncio.sleep(delay)
        if self.cursors_on and cur["id"]:
            exec_cmd("raise_window", id=cur["id"])
        await self.bus.emit("cursor", actor=actor, x=cur["x"], y=cur["y"], phase="end")
        return {"actor": actor, "x": cur["x"], "y": cur["y"], "window_id": cur["id"]}

    async def hand(self, actor: Optional[str], x: int, y: int) -> None:
        if actor in ACTORS:
            await self.cursor_to(actor, x, y, ms=260)

    # typed reveal -----------------------------------------------------------
    async def type_text(self, win_id: str, req: TypeReq) -> Dict[str, Any]:
        live = windows()
        if win_id not in live:
            raise HTTPException(404, "window not found")
        w = live[win_id]
        await self.hand(req.actor, w["x"] + 2, w["y"] + 1)
        rng = random.Random(len(req.content))
        base = 1.0 / max(0.5, req.cps)
        typed, pending, sent = "", "", 0
        t_next = time.time()
        for i, ch in enumerate(req.content):
            typed += ch
            if req.target == "title":
                exec_cmd("window_title", id=win_id, title=typed)
            elif req.target == "figlet":
                exec_cmd("figlet_set_text", id=w["title"] or win_id, text=typed)
            else:
                # The editor drops a lone newline: carry it with the next character.
                if ch == "\n" and i + 1 < len(req.content):
                    pending += ch
                    continue
                mode = req.mode if sent == 0 else "append"
                resp = ipc("send_text", {"id": win_id, "content": pending + ch, "mode": mode,
                                         "position": req.position})
                pending = ""
                sent += 1
                if resp.lower().startswith("err"):
                    raise HTTPException(502, resp)
            await self.bus.emit("type", actor=req.actor, window=w, char=ch, i=i, n=len(req.content),
                                target=req.target)
            gap = base * (1 + req.jitter * (rng.random() * 2 - 1))
            if ch in ".,!?;:\n":
                gap *= 3
            t_next += gap
            delay = t_next - time.time()
            if delay > 0:
                await asyncio.sleep(delay)
        return {"ok": True, "window_id": win_id, "chars": len(req.content)}

    # glide ---------------------------------------------------------------
    async def glide(self, win_id: str, req: GlideReq) -> Dict[str, Any]:
        live = windows()
        if win_id not in live:
            raise HTTPException(404, "window not found")
        w = dict(live[win_id])
        await self.hand(req.actor, w["x"] + w["w"] // 2, w["y"])
        x0, y0, w0, h0 = w["x"], w["y"], w["w"], w["h"]
        w1, h1 = req.w if req.w is not None else w0, req.h if req.h is not None else h0
        steps = max(1, int(req.ms / 1000 * max(1, req.fps)))
        await self.bus.emit("move", actor=req.actor, window=w, phase="start", to={"x": req.x, "y": req.y},
                            ms=req.ms, capture=False)
        t_start = time.time()
        last = (x0, y0, w0, h0)
        for i in range(1, steps + 1):
            u = ease(req.ease, i / steps)
            nx, ny = round(x0 + (req.x - x0) * u), round(y0 + (req.y - y0) * u)
            nw, nh = round(w0 + (w1 - w0) * u), round(h0 + (h1 - h0) * u)
            if (nx, ny, nw, nh) != last:
                spec = f"{win_id},{nx},{ny}" + (f",{nw},{nh}" if (nw, nh) != (w0, h0) or (w1, h1) != (w0, h0) else "")
                ipc("move_many", {"moves": spec})
                if self.cursors_on and req.actor in self.cursors and self.cursors[req.actor].get("id"):
                    cur = self.cursors[req.actor]
                    cur["x"], cur["y"] = nx + nw // 2, ny
                    ipc("move_many", {"moves": f"{cur['id']},{cur['x']},{cur['y']}"})
                last = (nx, ny, nw, nh)
                w.update(x=nx, y=ny, w=nw, h=nh)
                if i < steps:
                    await self.bus.emit("move", actor=req.actor, window=w, phase="step")
            delay = t_start + i * req.ms / 1000 / steps - time.time()
            if delay > 0:
                await asyncio.sleep(delay)
        await self.bus.emit("move", actor=req.actor, window=w, phase="end")
        return {"ok": True, "window_id": win_id, "x": w["x"], "y": w["y"], "w": w["w"], "h": w["h"]}

    # droste ---------------------------------------------------------------
    async def droste(self, req: DrosteReq) -> Dict[str, Any]:
        cols, rows = desk()
        scr_rows = rows + 2
        W = req.w or max(10, int(cols * 0.6))
        H = req.h or max(5, int(rows * 0.6))
        X = req.x if req.x is not None else (cols - W) // 2
        Y = req.y if req.y is not None else (rows - H) // 2
        await self.hand(req.actor, X, Y)
        ids, rects = [], []
        rect = (X, Y, W, H)
        for level in range(req.depth):
            x, y, w, h = rect
            if w < 6 or h < 3:
                break
            grid = screen_cells(capture_text(), cols)[:scr_rows]
            lines = downsample(grid, w - 3, h - 2)
            title = req.title if level == 0 else ""
            wid = open_text_window(lines, x, y, w, h, title=title, name=f"droste-{level}")
            if not wid:
                break
            ids.append(wid)
            rects.append(rect)
            await self.bus.emit("droste", actor=req.actor, window={"id": wid, "title": title, "x": x, "y": y,
                                "w": w, "h": h}, level=level, depth=req.depth)
            await asyncio.sleep(req.pause_ms / 1000)
            # the same window, seen inside itself: map screen -> this window's interior
            # Next level: `scale` of this window, placed inside its interior at the same
            # relative spot this window holds on the desktop (an exact screen->interior map
            # shrinks rows too fast at terminal resolution: frames eat two rows a level).
            nw, nh = round(w * req.scale), round(h * req.scale)
            nx = x + 1 + round(x / max(1, cols - w) * max(0, w - 2 - nw)) if cols > w else x + 1
            ny = y + 1 + round(y / max(1, rows - h) * max(0, h - 2 - nh)) if rows > h else y + 1
            rect = (nx, ny, nw, nh)
        bottom = None
        if req.last and rects:
            x, y, w, h = rect
            lines = req.last.split("\n")
            lw = max(sum(cw(c) for c in l) for l in lines)
            bw, bh = lw + 1, len(lines)
            bx = x + max(0, (w - bw) // 2)
            by = y + max(0, (h - bh) // 2)
            bottom = open_text_window(lines, bx, by, bw, bh, frameless=True, shadowless=True, name="droste-last")
            if bottom:
                ids.append(bottom)
                await self.bus.emit("droste", actor=req.actor, window={"id": bottom, "title": "", "x": bx,
                                    "y": by, "w": bw, "h": bh}, level=len(rects), depth=req.depth, last=True)
        return {"ok": True, "window_ids": ids, "levels": len(rects), "bottom": bottom}

    # shape ---------------------------------------------------------------
    async def shape(self, req: ShapeReq) -> Dict[str, Any]:
        rows_txt = MASKS.get(req.mask) or [l for l in req.mask.split("\n")]
        rows_txt = [r.rstrip() for r in rows_txt]
        while rows_txt and not rows_txt[-1].strip():
            rows_txt.pop()
        mh = len(rows_txt)
        mw = max((len(r) for r in rows_txt), default=0)
        if not mh or not mw:
            raise HTTPException(400, "empty mask")
        cols, drows = desk()
        if req.cell:
            cwid, chei = (int(v) for v in req.cell.lower().split("x"))
        else:
            cwid = max(3, min(16, int(cols * 0.8) // mw))
            chei = max(2, min(8, int(drows * 0.85) // mh))
        ox = req.x if req.x is not None else max(0, (cols - mw * cwid) // 2)
        oy = req.y if req.y is not None else max(0, (drows - mh * chei) // 2)
        cells = [(c, r) for r in range(mh) for c in range(mw) if c < len(rows_txt[r])
                 and rows_txt[r][c] not in " ."]
        if req.order == "random":
            random.Random(mw * 31 + mh).shuffle(cells)
        elif req.order == "cols":
            cells.sort()
        elif req.order == "spiral":
            cx, cy = (mw - 1) / 2, (mh - 1) / 2
            cells.sort(key=lambda p: (math.hypot((p[0] - cx), (p[1] - cy) * 2), math.atan2(p[1] - cy, p[0] - cx)))
        await self.hand(req.actor, ox, oy)
        ids = []
        for k, (c, r) in enumerate(cells):
            x, y = ox + c * cwid, oy + r * chei
            inner_w = max(1, cwid - (0 if req.frameless else 2))
            inner_h = max(1, chei - (0 if req.frameless else 2))
            if req.primers:
                src = req.primers[k % len(req.primers)]
                try:
                    lines = Path(src).read_text(encoding="utf-8").split("\n")
                except OSError:
                    lines = []
                lines = [l[:inner_w] for l in lines[:inner_h]]
            else:
                ch = rows_txt[r][c] if req.fill == "mask" else (req.fill[:1] or " ")
                lines = [ch * inner_w] * inner_h
            title = req.titles[k] if k < len(req.titles) else ""
            wid = open_text_window(lines, x, y, cwid, chei, title=title, frameless=req.frameless,
                                   name=f"shape-{k}")
            if wid:
                ids.append(wid)
                await self.bus.emit("open", actor=req.actor, window={"id": wid, "title": title, "x": x, "y": y,
                                    "w": cwid, "h": chei}, shape=req.mask if req.mask in MASKS else "custom",
                                    cell=[c, r], index=k, n=len(cells))
            if req.stagger_ms:
                await asyncio.sleep(req.stagger_ms / 1000)
        await self.bus.emit("shape", actor=req.actor, ids=ids, cell=[cwid, chei], origin=[ox, oy],
                            mask_size=[mw, mh])
        return {"ok": True, "window_ids": ids, "cell": [cwid, chei], "origin": [ox, oy]}

    # gravity ---------------------------------------------------------------
    async def fall(self, req: FallReq) -> Dict[str, Any]:
        cols, rows = desk()
        live = windows()
        skip = self.cursor_ids()
        ids = [i for i in (req.ids or list(live)) if i in live and i not in skip]
        ids.sort(key=lambda i: -(live[i]["y"] + live[i]["h"]))     # lowest first
        ph = GridPhysics(rows, cols, g=req.g, floor_bounce=req.bounce, repose=req.repose,
                         wall_bounce=0.4, drag=0.3, seed=req.seed)
        rng = random.Random(req.seed)
        pid_of: Dict[int, str] = {}
        for i in ids:
            w = live[i]
            if w["w"] > cols or w["h"] > rows:
                continue
            pid = ph.add(min(rows - 1, w["y"] + w["h"] - 1), max(0, min(cols - w["w"], w["x"])),
                         width=w["w"], weight=max(1.0, w["w"] * w["h"] / 40))
            pid_of[pid] = i
            ph.release(pid, vy=0.0, vx=rng.uniform(-req.jitter, req.jitter))
        await self.hand(req.actor, cols // 2, rows - 1)
        await self.bus.emit("fall", actor=req.actor, phase="start", ids=list(pid_of.values()), capture=False)
        dt = 1.0 / max(1, req.fps)
        t, order, lands = 0.0, 0, []
        pos = {p: (live[pid_of[p]]["x"], live[pid_of[p]]["y"]) for p in pid_of}
        t_start = time.time()
        while t < req.max_s:
            evs = ph.step(t, dt)
            moves = []
            for p, wid in pid_of.items():
                w = live[wid]
                r, c = ph.cell(p)
                nx, ny = c, r - w["h"] + 1
                if (nx, ny) != pos[p]:
                    pos[p] = (nx, ny)
                    moves.append(f"{wid},{nx},{ny}")
            if moves:
                ipc("move_many", {"moves": ";".join(moves)})
            for e in evs:
                kind, _, p, top, c0 = e
                wid = pid_of[p]
                w = dict(live[wid], x=pos[p][0], y=pos[p][1])
                if kind == "land":
                    # a window is h rows tall: raise the pile by its full height
                    h = w["h"]
                    for cc in range(c0, min(cols, c0 + w["w"])):
                        ph.heights[cc] = max(ph.heights[cc], rows - 1 - top + h)
                    order += 1
                    lands.append(wid)
                    await self.bus.emit("land", actor=req.actor, window=w, order=order, n=len(pid_of),
                                        impact=round(abs(ph.vy[p]), 2), col=c0, row=top,
                                        col_norm=round(c0 / max(1, cols - 1), 3),
                                        row_norm=round(top / max(1, rows - 1), 3))
                else:
                    await self.bus.emit("land", actor=req.actor, window=w, phase="bounce",
                                        impact=round(abs(ph.vy[p]), 2), col=c0, row=top, capture=False)
            if moves:
                await self.bus.emit("fall", actor=req.actor, phase="step", moved=len(moves))
            if all(ph.state(p) == "landed" for p in pid_of):
                break
            t += dt
            delay = t_start + t - time.time()
            if delay > 0:
                await asyncio.sleep(delay)
        await self.bus.emit("fall", actor=req.actor, phase="end", landed=len(lands), n=len(pid_of))
        return {"ok": True, "landed": lands, "n": len(pid_of), "seconds": round(t, 2)}

    # chrome ---------------------------------------------------------------
    async def chrome(self, req: ChromeReq) -> Dict[str, Any]:
        if req.reset:
            exec_cmd("chrome_set", part="reset")
            await self.bus.emit("chrome", actor=req.actor, part="reset")
            return {"ok": True}
        done = {}
        for part in ("menu", "status", "kaomoji"):
            text = getattr(req, part)
            if text is None:
                continue
            if req.cps > 0 and part != "kaomoji":
                for i in range(1, len(text) + 1):
                    r = exec_cmd("chrome_set", part=part, text=text[:i] or " ")
                    await self.bus.emit("chrome", actor=req.actor, part=part, text=text[:i], phase="step")
                    await asyncio.sleep(1.0 / req.cps)
            else:
                r = exec_cmd("chrome_set", part=part, text=text)
            if r.lower().startswith("err"):
                raise HTTPException(501, f"this WIBWOB-DOS build has no chrome_set ({r})")
            done[part] = text
            await self.bus.emit("chrome", actor=req.actor, part=part, text=text)
        return {"ok": True, "set": done}


# ───────────────────────────── routes ─────────────────────────────

def register(app: FastAPI, events_hub) -> ShowBus:
    bus = ShowBus()
    show = Show(bus)
    events_hub.add_listener(bus.legacy)
    app.state.show_bus = bus
    app.state.show = show

    @app.get("/events", summary="Show event stream (SSE): every action with window geometry")
    async def events_stream(request: Request, types: str = "") -> StreamingResponse:
        want = set(t for t in types.split(",") if t)
        q: asyncio.Queue = asyncio.Queue()
        bus.subs.add(q)

        async def gen():
            try:
                yield ": wibwob-dos show events\n\n"
                while True:
                    if await request.is_disconnected():
                        break
                    try:
                        ev = await asyncio.wait_for(q.get(), timeout=10)
                    except asyncio.TimeoutError:
                        yield ": keepalive\n\n"
                        continue
                    if want and ev["type"] not in want:
                        continue
                    yield f"event: {ev['type']}\nid: {ev['seq']}\ndata: {json.dumps(ev, ensure_ascii=False)}\n\n"
            finally:
                bus.subs.discard(q)
        return StreamingResponse(gen(), media_type="text/event-stream",
                                 headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"})

    @app.get("/events/recent")
    async def events_recent(n: int = 100, after: int = 0) -> Dict[str, Any]:
        evs = [e for e in bus.ring if e["seq"] > after][-n:]
        return {"events": evs, "last_seq": bus.seq}

    @app.post("/events/mark")
    async def events_mark(payload: Dict[str, Any]) -> Dict[str, Any]:
        """Put a named marker (a beat, a spoken line) on the stream and the timeline."""
        ev = await bus.emit("mark", actor=payload.get("actor"), label=str(payload.get("label", "")),
                            text=str(payload.get("text", "")))
        return {"ok": True, "seq": ev["seq"]}

    @app.post("/capture/start")
    async def capture_start(req: CaptureStart) -> Dict[str, Any]:
        if bus.rec:
            bus.rec.events.close()
            bus.rec.frames.close()
        stamp = time.strftime("%Y%m%d-%H%M%S")
        run = Path(req.dir) if req.dir else Path(os.environ.get(
            "WIBWOB_REPO_ROOT", Path(__file__).resolve().parents[2])) / "logs" / "show" / f"{stamp}-{req.name}"
        bus.rec = Recorder(run, req.settle_ms, dict(req.every))
        await bus.emit("capture", phase="start", run_dir=str(run))
        p = await bus.rec.frame("start")
        return {"ok": True, "run_dir": str(run), "first": p.name}

    @app.post("/capture/frame")
    async def capture_frame(payload: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        if not bus.rec:
            raise HTTPException(409, "no capture running (POST /capture/start)")
        label = str((payload or {}).get("label", "frame")).replace("/", "-")[:40]
        p = await bus.rec.frame(label)
        return {"ok": True, "frame": p.name}

    @app.post("/capture/stop")
    async def capture_stop() -> Dict[str, Any]:
        if not bus.rec:
            raise HTTPException(409, "no capture running")
        rec = bus.rec
        await rec.frame("end")
        await bus.emit("capture", phase="stop", frames=rec.n, capture=False)
        rec.events.close()
        rec.frames.close()
        bus.rec = None
        return {"ok": True, "run_dir": str(rec.run_dir), "frames": rec.n}

    @app.get("/capture/status")
    async def capture_status() -> Dict[str, Any]:
        return {"running": bool(bus.rec), "run_dir": str(bus.rec.run_dir) if bus.rec else None,
                "frames": bus.rec.n if bus.rec else 0}

    @app.post("/windows/{win_id}/type")
    async def type_route(win_id: str, req: TypeReq) -> Dict[str, Any]:
        return await show.type_text(win_id, req)

    @app.post("/windows/{win_id}/glide")
    async def glide_route(win_id: str, req: GlideReq) -> Dict[str, Any]:
        return await show.glide(win_id, req)

    @app.post("/cursors/on")
    async def cursors_on(payload: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Draw the hands on the desktop as tiny windows (they are always on the event stream)."""
        show.cursors_on = bool((payload or {}).get("on", True))
        if not show.cursors_on:
            for c in show.cursors.values():
                if c.get("id"):
                    ipc("close_window", {"id": c["id"]})
                    c["id"] = None
        return {"ok": True, "on": show.cursors_on}

    @app.post("/cursors/{actor}")
    async def cursor_route(actor: str, req: CursorReq) -> Dict[str, Any]:
        if actor not in ACTORS:
            raise HTTPException(400, f"actor must be one of {ACTORS}")
        return await show.cursor_to(actor, req.x, req.y, req.ms, req.ease, req.glyph)

    @app.get("/cursors")
    async def cursors_list() -> Dict[str, Any]:
        return {"on": show.cursors_on, "cursors": show.cursors}

    @app.post("/windows/droste")
    async def droste_route(req: DrosteReq) -> Dict[str, Any]:
        return await show.droste(req)

    @app.get("/windows/shape/masks")
    async def masks_route() -> Dict[str, Any]:
        return {"masks": {k: "\n".join(v) for k, v in MASKS.items()}}

    @app.post("/windows/shape")
    async def shape_route(req: ShapeReq) -> Dict[str, Any]:
        return await show.shape(req)

    @app.post("/windows/fall")
    async def fall_route(req: FallReq) -> Dict[str, Any]:
        return await show.fall(req)

    @app.post("/paint/bare")
    async def paint_bare(payload: Dict[str, Any]) -> Dict[str, Any]:
        """Paint window as canvas only (no tool panel, palette, menu or status). Clears it."""
        wid = str(payload.get("win_id", ""))
        r = exec_cmd("paint_bare", id=wid, on=1 if payload.get("on", True) else 0)
        if r.lower().startswith("err"):
            raise HTTPException(404 if "not found" in r else 501, r)
        await bus.emit("chrome", actor=payload.get("actor"), part="paint_bare", window={"id": wid})
        return {"ok": True, "win_id": wid}

    @app.post("/chrome")
    async def chrome_route(req: ChromeReq) -> Dict[str, Any]:
        return await show.chrome(req)

    return bus
