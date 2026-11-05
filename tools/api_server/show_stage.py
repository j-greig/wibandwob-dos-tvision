"""Film windows and nested WIBWOB-DOS desktops for live shows (GLITCH talk).

- POST /show/film: a terminal window that plays a film tape (tools/tapes/tape_play.py) at a
  wall-clock epoch, so the talk player can start the tape's mp3 at the same moment.
- POST /show/nest: a terminal window running a second WIBWOB-DOS (instance nest<k>);
  POST /show/inner/{instance}/{verb} acts inside it over its own IPC socket, including
  `nest`, which opens a deeper one inside that desktop. POST /show/nest/close_all ends them.

Both use the app's `open_terminal exec=<cmd>`: the window runs <cmd> through
tools/term_exec.sh with no shell prompt, no echo and no cursor.

Events go through the show bus (GET /events), same envelope as every other show event:
  {"seq","t","type":"film","actor","window":{...},"data":{"phase":"start"|"end","tape","at",
   "duration","fps","audio","title"}}
  {"seq","t","type":"nest","actor","window":{...},"data":{"phase":"open"|"close","instance",
   "depth","parent"}}
"""
from __future__ import annotations

import asyncio
import base64
import contextlib
import json
import os
import shlex
import signal
import socket
import subprocess
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

from . import show as show_mod

ROOT = Path(os.environ.get("WIBWOB_REPO_ROOT", Path(__file__).resolve().parents[2]))
NEST_MAX_DEPTH = 6


def tapes_dir() -> Path:
    return Path(os.environ.get("WWDOS_TAPES", ROOT / "modules" / "wibwob-2026" / "tapes"))


def tape_player() -> Path:
    return Path(os.environ.get("WWDOS_TAPE_PLAYER", ROOT / "tools" / "tapes" / "tape_play.py"))


def nest_binary() -> str:
    """The WIBWOB-DOS binary an inner desktop runs (WWDOS_NEST_BIN, default build/app/wwdos)."""
    return os.environ.get("WWDOS_NEST_BIN", str(ROOT / "build" / "app" / "wwdos"))


def sock_path(instance: str) -> str:
    return f"/tmp/wibwob_{instance}.sock"


# ───────────────────────────── IPC to a named socket ─────────────────────────────

def ipc_at(sock: str, cmd: str, kv: Optional[Dict[str, Any]] = None, timeout: float = 5.0) -> str:
    """One IPC call to a specific instance's socket (same wire format as ipc_client.send_cmd)."""
    parts = [f"cmd:{cmd}"]
    for k, v in (kv or {}).items():
        v = str(v)
        if k == "content" and ("\n" in v or "\r" in v or " " in v):
            parts.append(f"{k}=base64:" + base64.b64encode(v.encode("utf-8")).decode("ascii"))
        else:
            parts.append(f"{k}=" + v.replace(" ", "%20").replace("\n", "%0A").replace("\r", "%0D"))
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    try:
        s.connect(sock)
        s.sendall((" ".join(parts) + "\n").encode("utf-8"))
        chunks = []
        while True:
            c = s.recv(65536)
            if not c:
                break
            chunks.append(c)
        return b"".join(chunks).decode("utf-8", errors="ignore").strip()
    finally:
        s.close()


def exec_at(sock: str, name: str, **args: Any) -> str:
    kv = {"name": name, "actor": "show"}
    kv.update({k: v for k, v in args.items() if v is not None})
    return ipc_at(sock, "exec_command", kv)


def sock_alive(sock: str) -> bool:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(0.3)
    try:
        s.connect(sock)
        return True
    except OSError:
        return False
    finally:
        s.close()


def state_at(sock: str) -> Dict[str, Any]:
    try:
        return json.loads(ipc_at(sock, "get_state") or "{}")
    except (json.JSONDecodeError, OSError):
        return {}


def desk_at(sock: str) -> tuple:
    try:
        c = json.loads(ipc_at(sock, "get_canvas_size") or "{}")
        return int(c.get("width", 80)), int(c.get("height", 24)) - 1
    except (json.JSONDecodeError, OSError):
        return 80, 23


def parse_id(resp: str) -> Optional[str]:
    try:
        return json.loads(resp).get("id")
    except (json.JSONDecodeError, AttributeError):
        return None


def ancestors(pid: int, limit: int = 12) -> List[int]:
    """Parent, grandparent, ... of a process."""
    out: List[int] = []
    for _ in range(limit):
        try:
            r = subprocess.run(["ps", "-o", "ppid=", "-p", str(pid)], capture_output=True, text=True, timeout=2)
            pid = int(r.stdout.strip() or 0)
        except (OSError, ValueError, subprocess.TimeoutExpired):
            break
        if pid <= 1:
            break
        out.append(pid)
    return out


def sock_pids(sock: str) -> List[int]:
    """Processes holding a socket (the instance that serves it)."""
    try:
        r = subprocess.run(["lsof", "-t", sock], capture_output=True, text=True, timeout=3)
        return [int(p) for p in r.stdout.split() if p.strip().isdigit()]
    except (OSError, subprocess.TimeoutExpired):
        return []


# ───────────────────────────── request models ─────────────────────────────

class FilmReq(BaseModel):
    tape: str
    x: Optional[int] = None
    y: Optional[int] = None
    title: Optional[str] = None
    actor: Optional[str] = None
    lead_ms: int = 1200
    close_after: bool = False


class NestReq(BaseModel):
    x: Optional[int] = None
    y: Optional[int] = None
    w: Optional[int] = None
    h: Optional[int] = None
    title: Optional[str] = None
    theme: str = "ega"
    actor: Optional[str] = None
    boot_s: float = 10.0


class InnerReq(BaseModel):
    """Body of POST /show/inner/{instance}/{verb}; each verb reads the fields it needs."""
    id: Optional[str] = None            # inner window id (move, glide, type)
    path: Optional[str] = None          # primer: file (bare name = the shelf primers dir)
    text: Optional[str] = None          # figlet text; chrome text
    content: Optional[str] = None       # type: the text
    font: Optional[str] = None
    x: Optional[int] = None
    y: Optional[int] = None
    w: Optional[int] = None
    h: Optional[int] = None
    title: Optional[str] = None
    colour: Optional[str] = None
    ink: Optional[int] = None
    frameless: bool = False
    cps: float = 14.0
    target: str = "editor"              # type: editor | title | figlet
    ms: int = 800                       # glide
    part: Optional[str] = None          # chrome: menu | status | kaomoji | reset
    theme: str = "ega"                  # nest
    actor: Optional[str] = None
    boot_s: float = 10.0


# ───────────────────────────── the stage ─────────────────────────────

class Stage:
    def __init__(self, bus: "show_mod.ShowBus", show: "show_mod.Show") -> None:
        self.bus = bus
        self.show = show
        # instance -> {instance, depth, parent, window_id (in the parent desktop), sock, rect}
        self.nests: Dict[str, Dict[str, Any]] = {}
        self.films: Dict[str, Dict[str, Any]] = {}
        self._tasks: set = set()

    # tapes -----------------------------------------------------------------
    def tape_index(self) -> Dict[str, Dict[str, Any]]:
        p = tapes_dir() / "INDEX.json"
        try:
            data = json.loads(p.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise HTTPException(503, f"no tape index at {p}: {exc}")
        items = data.get("tapes", data) if isinstance(data, dict) else data
        return {t["slug"]: t for t in items if isinstance(t, dict) and t.get("slug")}

    def _spawn(self, coro) -> None:
        t = asyncio.ensure_future(coro)
        self._tasks.add(t)
        t.add_done_callback(self._tasks.discard)

    # film ------------------------------------------------------------------
    async def film(self, req: FilmReq) -> Dict[str, Any]:
        idx = self.tape_index()
        tape = idx.get(req.tape)
        if not tape:
            raise HTTPException(404, f"no tape {req.tape!r}; have: {', '.join(sorted(idx))}")
        cols, rows = int(tape["cols"]), int(tape["rows"])
        fps = float(tape["fps"])
        duration = float(tape["duration"])
        tfile = tapes_dir() / tape["file"]
        if not tfile.exists():
            raise HTTPException(503, f"tape file missing: {tfile}")
        audio = str((tapes_dir() / tape["audio"]).resolve()) if tape.get("audio") else None
        dcols, drows = show_mod.desk()
        w, h = cols + 2, rows + 2
        x = req.x if req.x is not None else (dcols - w) // 2
        y = req.y if req.y is not None else (drows - h) // 2
        x = max(0, min(x, max(0, dcols - w)))
        y = max(0, min(y, max(0, drows - h)))
        title = req.title if req.title is not None else tape.get("title", req.tape)
        await self.show.hand(req.actor, x + w // 2, y)
        at = round(time.time() + max(0, req.lead_ms) / 1000.0, 3)
        cmd = "exec python3 {} {} --at {:.3f}".format(
            shlex.quote(str(tape_player())), shlex.quote(str(tfile)), at)
        resp = show_mod.exec_cmd("open_terminal", exec=cmd, title=title, x=x, y=y, w=w, h=h,
                                 hold=0 if req.close_after else 1)
        if resp.lower().startswith("err"):
            raise HTTPException(502, resp)
        wid = parse_id(resp)
        win = {"id": wid, "title": title, "x": x, "y": y, "w": w, "h": h}
        await self.bus.emit("film", actor=req.actor, window=win, phase="start", tape=req.tape, at=at,
                            duration=duration, fps=fps, audio=audio, title=title)
        self.films[wid or req.tape] = {"tape": req.tape, "at": at, "duration": duration, "window": win}
        self._spawn(self._film_end(wid, win, req, at, duration))
        return {"window_id": wid, "at": at, "duration": duration, "fps": fps, "audio": audio,
                "title": title, "cols": cols, "rows": rows, "x": x, "y": y, "w": w, "h": h}

    async def _film_end(self, wid, win, req: FilmReq, at: float, duration: float) -> None:
        delay = at + duration - time.time()
        if delay > 0:
            await asyncio.sleep(delay)
        closed = False
        if req.close_after and wid:
            await asyncio.sleep(0.15)   # let the last frame land before the window goes
            show_mod.ipc("close_window", {"id": wid})
            closed = True
            await self.bus.emit("close", actor=req.actor, window=win, source="film")
        self.films.pop(wid or req.tape, None)
        await self.bus.emit("film", actor=req.actor, window=win, phase="end", tape=req.tape, at=at,
                            duration=duration, closed=closed)

    # nest ------------------------------------------------------------------
    def adopt(self) -> None:
        """Take back nested desktops this app is running that the API does not know about
        (the API was restarted while they ran). A nest counts as ours when its process
        descends from the app that serves our own socket; other apps' nests are left alone."""
        import glob
        from .ipc_client import get_sock_path
        outer = sock_pids(get_sock_path())
        if not outer:
            return
        found: Dict[str, Dict[str, Any]] = {}
        for sock in glob.glob("/tmp/wibwob_nest*.sock"):
            instance = Path(sock).stem.replace("wibwob_", "", 1)
            if instance in self.nests or not sock_alive(sock):
                continue
            pids = sock_pids(sock)
            if not pids:
                continue
            chain = ancestors(pids[0])
            if outer[0] not in chain:
                continue
            found[instance] = {"pid": pids[0], "chain": chain, "sock": sock}
        by_pid = {v["pid"]: k for k, v in found.items()}
        for instance, f in found.items():
            nest_parents = [by_pid[p] for p in f["chain"] if p in by_pid]
            parent = nest_parents[0] if nest_parents else None
            title = f"WIBWOB-DOS ({instance})"
            psock = sock_path(parent) if parent else None
            wins = state_at(psock).get("windows", []) if psock else \
                [{"id": i, "title": w.get("title")} for i, w in show_mod.windows().items()]
            wid = next((w["id"] for w in wins if w.get("title") == title), None)
            self.nests[instance] = {"instance": instance, "depth": len(nest_parents) + 1, "parent": parent,
                                    "window_id": wid, "sock": f["sock"], "rect": None, "title": title,
                                    "adopted": True}

    def next_instance(self) -> str:
        k = 1
        while f"nest{k}" in self.nests or sock_alive(sock_path(f"nest{k}")):
            k += 1
        return f"nest{k}"

    def nest_command(self, instance: str, theme: str) -> str:
        theme = theme if theme in ("ega", "monochrome", "mono", "dark_pastel") else "ega"
        log = f"/tmp/wwdos-{instance}.log"
        return (f"WIBWOB_INSTANCE={shlex.quote(instance)} WWDOS_THEME={shlex.quote(theme)} "
                f"exec {shlex.quote(nest_binary())} 2>>{shlex.quote(log)}")

    async def _wait_sock(self, sock: str, boot_s: float) -> bool:
        t_end = time.time() + boot_s
        while time.time() < t_end:
            if os.path.exists(sock) and sock_alive(sock):
                # the socket answers before the first draw; give the desktop a moment
                await asyncio.sleep(0.4)
                return True
            await asyncio.sleep(0.15)
        return False

    async def nest(self, req: NestReq) -> Dict[str, Any]:
        """Open a terminal window on the main desktop and boot WIBWOB-DOS inside it."""
        if not Path(nest_binary()).exists():
            raise HTTPException(503, f"no WIBWOB-DOS binary at {nest_binary()} (set WWDOS_NEST_BIN)")
        dcols, drows = show_mod.desk()
        w = req.w or max(40, int(dcols * 0.62))
        h = req.h or max(14, int(drows * 0.78))
        w, h = min(w, dcols), min(h, drows)
        x = req.x if req.x is not None else (dcols - w) // 2
        y = req.y if req.y is not None else (drows - h) // 2
        instance = self.next_instance()
        sock = sock_path(instance)
        with contextlib.suppress(OSError):
            os.unlink(sock)
        title = req.title if req.title is not None else f"WIBWOB-DOS ({instance})"
        await self.show.hand(req.actor, x + w // 2, y)
        resp = show_mod.exec_cmd("open_terminal", exec=self.nest_command(instance, req.theme),
                                 title=title, x=x, y=y, w=w, h=h, hold=0)
        if resp.lower().startswith("err"):
            raise HTTPException(502, resp)
        wid = parse_id(resp)
        rec = {"instance": instance, "depth": 1, "parent": None, "window_id": wid, "sock": sock,
               "rect": {"x": x, "y": y, "w": w, "h": h}, "title": title}
        self.nests[instance] = rec
        ok = await self._wait_sock(sock, req.boot_s)
        if not ok:
            await self._close_one(instance)
            raise HTTPException(504, f"{instance} did not open {sock} within {req.boot_s}s "
                                     f"(see /tmp/wwdos-{instance}.log)")
        win = {"id": wid, "title": title, "x": x, "y": y, "w": w, "h": h}
        await self.bus.emit("nest", actor=req.actor, window=win, phase="open", instance=instance,
                            depth=1, parent=None)
        return {"window_id": wid, "instance": instance, "depth": 1, "x": x, "y": y, "w": w, "h": h,
                "inner": {"cols": w - 2, "rows": h - 2}}

    def _nest_rec(self, instance: str) -> Dict[str, Any]:
        if instance not in self.nests:
            self.adopt()
        rec = self.nests.get(instance)
        if not rec:
            raise HTTPException(404, f"no nested desktop {instance!r}; open: {sorted(self.nests)}")
        if not sock_alive(rec["sock"]):
            raise HTTPException(410, f"{instance} is not answering on {rec['sock']}")
        return rec

    async def inner(self, instance: str, verb: str, req: InnerReq) -> Dict[str, Any]:
        rec = self._nest_rec(instance)
        sock = rec["sock"]
        if verb == "primer":
            if not req.path:
                raise HTTPException(400, "primer needs path")
            path = req.path
            if "/" not in path:
                path = str(ROOT / "modules" / "wibwob-2026" / "primers" / path)
            args = dict(path=path, title=req.title or "", colour=req.colour,
                        ink=req.ink if req.ink is not None else None)
            if None not in (req.x, req.y, req.w, req.h):
                args.update(x=req.x, y=req.y, w=req.w, h=req.h)
            if req.frameless:
                args["frameless"] = 1
            before = set(w["id"] for w in state_at(sock).get("windows", []))
            r = exec_at(sock, "open_primer", **args)
            if r.lower().startswith("err"):
                raise HTTPException(502, r)
            new = [w for w in state_at(sock).get("windows", []) if w["id"] not in before]
            wid = new[-1]["id"] if new else None
            await self._inner_event(req, rec, "primer", wid, name=Path(path).stem)
            return {"ok": True, "instance": instance, "window_id": wid}
        if verb == "figlet":
            kv = {"type": "figlet_text", "text": req.text or "WIBWOB", "font": req.font or "standard"}
            for k in ("x", "y", "w", "h"):
                if getattr(req, k) is not None:
                    kv[k] = getattr(req, k)
            if req.colour:
                kv["colour"] = req.colour
            if req.ink is not None:
                kv["ink"] = req.ink
            if req.title:
                kv["title"] = req.title
            before = set(w["id"] for w in state_at(sock).get("windows", []))
            r = ipc_at(sock, "create_window", kv)
            if r.lower().startswith("err"):
                raise HTTPException(502, r)
            wid = parse_id(r)
            if not wid:
                new = [w for w in state_at(sock).get("windows", []) if w["id"] not in before]
                wid = new[-1]["id"] if new else None
            await self._inner_event(req, rec, "figlet", wid, text=kv["text"], font=kv["font"])
            return {"ok": True, "instance": instance, "window_id": wid}
        if verb == "type":
            return await self._inner_type(instance, sock, req)
        if verb in ("move", "glide"):
            if not req.id or req.x is None or req.y is None:
                raise HTTPException(400, f"{verb} needs id, x, y")
            return await self._inner_glide(instance, sock, req, ms=0 if verb == "move" else req.ms)
        if verb == "chrome":
            part = req.part or "menu"
            if part == "reset":
                r = exec_at(sock, "chrome_set", part="reset")
            else:
                r = exec_at(sock, "chrome_set", part=part, text=req.text if req.text is not None else "")
            if r.lower().startswith("err"):
                raise HTTPException(501, r)
            await self._inner_event(req, rec, "chrome", None, part=part, text=(req.text or "")[:80])
            return {"ok": True, "instance": instance, "part": part}
        if verb == "nest":
            return await self._inner_nest(rec, req)
        if verb == "close_all":
            for child in [i for i, r in self.nests.items() if r["parent"] == instance]:
                await self._close_tree(child)
            ipc_at(sock, "close_all")
            return {"ok": True, "instance": instance}
        if verb == "state":
            return {"instance": instance, "state": state_at(sock), "desk": desk_at(sock)}
        raise HTTPException(400, f"unknown verb {verb!r} (primer, figlet, type, move, glide, chrome, "
                                 f"nest, close_all, state)")

    async def _inner_event(self, req: InnerReq, rec: Dict[str, Any], verb: str, wid: Optional[str], **data: Any) -> None:
        """One event per change made inside a nested desktop, so a client's log can show what a dive or an inner call
        did on the level it did it (the nested app has no event stream of its own)."""
        win = {"id": wid, "title": req.title, "x": req.x, "y": req.y, "w": req.w, "h": req.h} if wid else None
        await self.bus.emit("inner", actor=req.actor, window=win, verb=verb, instance=rec["instance"],
                            depth=rec["depth"], **data)

    async def _inner_type(self, instance: str, sock: str, req: InnerReq) -> Dict[str, Any]:
        content = req.content if req.content is not None else (req.text or "")
        wid = req.id
        if not wid:
            kv = {"type": "text_editor"}
            for k in ("x", "y", "w", "h"):
                if getattr(req, k) is not None:
                    kv[k] = getattr(req, k)
            if req.title:
                kv["title"] = req.title
            if req.colour:
                kv["colour"] = req.colour
            if req.ink is not None:
                kv["ink"] = req.ink
            before = set(w["id"] for w in state_at(sock).get("windows", []))
            wid = parse_id(ipc_at(sock, "create_window", kv))
            if not wid:   # some spawns do not report the id: find the new window
                new = [w for w in state_at(sock).get("windows", []) if w["id"] not in before]
                wid = new[-1]["id"] if new else None
            if not wid:
                raise HTTPException(502, "could not open an editor inside " + instance)
        gap = 1.0 / max(0.5, req.cps)
        typed, pending, sent = "", "", 0
        for i, ch in enumerate(content):
            typed += ch
            if req.target == "title":
                exec_at(sock, "window_title", id=wid, title=typed)
            elif req.target == "figlet":
                exec_at(sock, "figlet_set_text", id=wid, text=typed)
            else:
                if ch == "\n" and i + 1 < len(content):
                    pending += ch
                    continue
                ipc_at(sock, "send_text", {"id": wid, "content": pending + ch,
                                           "mode": "replace" if sent == 0 else "append",
                                           "position": "end"})
                pending, sent = "", sent + 1
            await asyncio.sleep(gap * (3 if ch in ".,!?;:\n" else 1))
        return {"ok": True, "instance": instance, "window_id": wid, "chars": len(content)}

    async def _inner_glide(self, instance: str, sock: str, req: InnerReq, ms: int) -> Dict[str, Any]:
        wins = {w["id"]: w for w in state_at(sock).get("windows", [])}
        w0 = wins.get(req.id)
        if not w0:
            raise HTTPException(404, f"no window {req.id} inside {instance}")
        x0, y0 = int(w0["x"]), int(w0["y"])
        ww0, hh0 = int(w0.get("w", w0.get("width", 0))), int(w0.get("h", w0.get("height", 0)))
        w1 = req.w if req.w is not None else ww0
        h1 = req.h if req.h is not None else hh0
        steps = max(1, int(ms / 1000 * 24))
        t0 = time.time()
        last = None
        for i in range(1, steps + 1):
            u = show_mod.ease("inout", i / steps)
            nx, ny = round(x0 + (req.x - x0) * u), round(y0 + (req.y - y0) * u)
            nw, nh = round(ww0 + (w1 - ww0) * u), round(hh0 + (h1 - hh0) * u)
            if (nx, ny, nw, nh) != last:
                spec = f"{req.id},{nx},{ny}" + (f",{nw},{nh}" if (w1, h1) != (ww0, hh0) else "")
                ipc_at(sock, "move_many", {"moves": spec})
                last = (nx, ny, nw, nh)
            if ms:
                delay = t0 + i * ms / 1000 / steps - time.time()
                if delay > 0:
                    await asyncio.sleep(delay)
        return {"ok": True, "instance": instance, "window_id": req.id, "x": req.x, "y": req.y}

    async def _inner_nest(self, parent: Dict[str, Any], req: InnerReq) -> Dict[str, Any]:
        depth = parent["depth"] + 1
        if depth > NEST_MAX_DEPTH:
            raise HTTPException(400, f"deeper than {NEST_MAX_DEPTH} levels")
        psock = parent["sock"]
        dcols, drows = desk_at(psock)
        w = req.w or max(24, int(dcols * 0.66))
        h = req.h or max(9, int(drows * 0.74))
        w, h = min(w, dcols), min(h, drows)
        x = req.x if req.x is not None else (dcols - w) // 2
        y = req.y if req.y is not None else (drows - h) // 2
        instance = self.next_instance()
        sock = sock_path(instance)
        with contextlib.suppress(OSError):
            os.unlink(sock)
        title = req.title if req.title is not None else f"WIBWOB-DOS ({instance})"
        r = exec_at(psock, "open_terminal", exec=self.nest_command(instance, req.theme), title=title,
                    x=x, y=y, w=w, h=h, hold=0)
        if r.lower().startswith("err"):
            raise HTTPException(502, r)
        wid = parse_id(r)
        self.nests[instance] = {"instance": instance, "depth": depth, "parent": parent["instance"],
                                "window_id": wid, "sock": sock, "rect": {"x": x, "y": y, "w": w, "h": h},
                                "title": title}
        if not await self._wait_sock(sock, req.boot_s):
            await self._close_one(instance)
            raise HTTPException(504, f"{instance} did not open {sock} within {req.boot_s}s")
        await self.bus.emit("nest", actor=req.actor, window={"id": wid, "title": title, "x": x, "y": y,
                            "w": w, "h": h}, phase="open", instance=instance, depth=depth,
                            parent=parent["instance"])
        return {"window_id": wid, "instance": instance, "depth": depth, "parent": parent["instance"],
                "x": x, "y": y, "w": w, "h": h, "inner": {"cols": w - 2, "rows": h - 2}}

    # closing -----------------------------------------------------------------
    async def _close_one(self, instance: str) -> None:
        rec = self.nests.pop(instance, None)
        if not rec:
            return
        # Close the window that holds it (in the main desktop or in its parent nest).
        if rec["window_id"]:
            with contextlib.suppress(OSError):
                if rec["parent"] is None:
                    show_mod.ipc("close_window", {"id": rec["window_id"]})
                elif rec["parent"] in self.nests or sock_alive(sock_path(rec["parent"])):
                    ipc_at(sock_path(rec["parent"]), "close_window", {"id": rec["window_id"]})
        # Closing the terminal hangs up its pty; make sure the instance is gone.
        for _ in range(10):
            if not sock_alive(rec["sock"]):
                break
            await asyncio.sleep(0.1)
        for pid in sock_pids(rec["sock"]):
            with contextlib.suppress(OSError):
                os.kill(pid, signal.SIGTERM)
        with contextlib.suppress(OSError):
            os.unlink(rec["sock"])
        await self.bus.emit("nest", window={"id": rec["window_id"], "title": rec.get("title")},
                            phase="close", instance=instance, depth=rec["depth"], parent=rec["parent"])

    async def _close_tree(self, instance: str) -> None:
        for child in [i for i, r in self.nests.items() if r["parent"] == instance]:
            await self._close_tree(child)
        await self._close_one(instance)

    async def close_all(self) -> List[str]:
        self.adopt()
        roots = [i for i, r in self.nests.items() if r["parent"] is None]
        closed = list(self.nests)
        for i in sorted(roots):
            await self._close_tree(i)
        for i in list(self.nests):   # orphans
            await self._close_one(i)
        return closed

    def close_all_sync(self) -> None:
        """Shutdown path: no event loop work, kill every inner instance."""
        for rec in list(self.nests.values()):
            for pid in sock_pids(rec["sock"]):
                with contextlib.suppress(OSError):
                    os.kill(pid, signal.SIGTERM)
            with contextlib.suppress(OSError):
                os.unlink(rec["sock"])
        self.nests.clear()


# ───────────────────────────── routes ─────────────────────────────

def register(app: FastAPI) -> Stage:
    stage = Stage(app.state.show_bus, app.state.show)
    app.state.stage = stage

    @app.get("/show/tapes", summary="The film tapes /show/film can play (modules/wibwob-2026/tapes/INDEX.json)")
    async def tapes_route() -> Dict[str, Any]:
        return {"dir": str(tapes_dir()), "tapes": list(stage.tape_index().values())}

    @app.post("/show/film", summary="Play a film tape in a terminal window, in sync with its mp3")
    async def film_route(req: FilmReq) -> Dict[str, Any]:
        return await stage.film(req)

    @app.post("/show/nest", summary="Boot a nested WIBWOB-DOS in a terminal window on the desktop")
    async def nest_route(req: NestReq) -> Dict[str, Any]:
        return await stage.nest(req)

    @app.get("/show/nest", summary="Nested desktops that are open")
    async def nest_list() -> Dict[str, Any]:
        stage.adopt()
        return {"nests": list(stage.nests.values())}

    @app.post("/show/nest/close_all", summary="End every nested WIBWOB-DOS and close its window")
    async def nest_close_all() -> Dict[str, Any]:
        return {"ok": True, "closed": await stage.close_all()}

    @app.post("/show/inner/{instance}/{verb}", summary="Act inside a nested desktop")
    async def inner_route(instance: str, verb: str, req: Optional[InnerReq] = None) -> Dict[str, Any]:
        return await stage.inner(instance, verb, req or InnerReq())

    async def _stage_shutdown() -> None:
        stage.close_all_sync()
    import warnings
    with warnings.catch_warnings():   # FastAPI marks on_event deprecated; main.py has no lifespan
        warnings.simplefilter("ignore", DeprecationWarning)
        app.router.on_event("shutdown")(_stage_shutdown)

    return stage
