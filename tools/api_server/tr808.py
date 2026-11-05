"""TR-808 drum machine routes and step-event relay.

- POST /tr808 {action, id?, ...}: runs `exec_command name=tr808` in the app (see README
  "TR-808 drum machine"). `action: "open"` opens a window when none exists.
- GET /tr808/state?id=: the window's state as JSON.
- The relay holds one IPC `subscribe_events` connection to the app and puts every
  tr808_step / tr808_transport event on the show bus, so a web page plays the hits from
  GET /events?types=tr808_step (SSE). Relay events never write capture frames.
  WWDOS_TR808_RELAY=0 turns the relay off (contract tests set it).
"""
from __future__ import annotations

import asyncio
import json
import os
import socket
import threading
import time
from typing import Any, Dict, Optional

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, ConfigDict

from .ipc_client import get_sock_path, send_cmd

RELAYED = ("tr808_step", "tr808_transport")


class Tr808Req(BaseModel):
    model_config = ConfigDict(extra="allow")
    action: str = "state"
    id: Optional[str] = None


def _kv(action: str, win_id: Optional[str], extra: Dict[str, Any]) -> Dict[str, str]:
    kv: Dict[str, str] = {"name": "tr808", "action": action, "actor": "api"}
    if win_id:
        kv["id"] = win_id
    for k, v in extra.items():
        if v is None:
            continue
        if isinstance(v, bool):
            v = "1" if v else "0"
        kv[k] = str(v)
    return kv


def run_action(action: str, win_id: Optional[str] = None, **extra: Any) -> Dict[str, Any]:
    resp = send_cmd("exec_command", _kv(action, win_id, extra))
    if resp is None:
        raise HTTPException(503, "app socket unavailable")
    if resp.startswith("err"):
        raise HTTPException(400, resp)
    try:
        return json.loads(resp)
    except json.JSONDecodeError:
        return {"ok": True, "raw": resp}


class Relay:
    """IPC event subscriber thread -> show bus (asyncio)."""

    def __init__(self, bus, loop: asyncio.AbstractEventLoop) -> None:
        self.bus = bus
        self.loop = loop
        self.stop = threading.Event()
        self.connected = False
        self.count = 0
        self.thread = threading.Thread(target=self._run, name="tr808-relay", daemon=True)

    def start(self) -> None:
        self.thread.start()

    def _run(self) -> None:
        while not self.stop.is_set():
            try:
                self._session()
            except OSError:
                pass
            self.connected = False
            self.stop.wait(1.0)

    def _session(self) -> None:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(2.0)
        try:
            s.connect(get_sock_path())
            s.sendall(b"cmd:subscribe_events\n")
            buf = b""
            while not self.stop.is_set():
                try:
                    chunk = s.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    return
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    self._line(line)
        finally:
            s.close()

    def _line(self, line: bytes) -> None:
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            return
        if msg.get("type") == "subscribed":
            self.connected = True
            return
        ev = msg.get("event")
        if ev not in RELAYED:
            return
        payload = msg.get("payload") or {}
        win = {"id": payload.get("id")} if payload.get("id") else None
        self.count += 1
        asyncio.run_coroutine_threadsafe(
            self.bus.emit(ev, window=win, capture=False, app_seq=msg.get("seq"),
                          relay_ms=round(time.time() * 1000 - payload.get("at", 0), 1), **payload),
            self.loop)


def register(app: FastAPI, bus) -> None:
    state: Dict[str, Any] = {"relay": None}

    @app.post("/tr808", summary="Drive the TR-808 drum machine (action + args)")
    async def tr808_action(req: Tr808Req) -> Dict[str, Any]:
        extra = dict(req.model_extra or {})
        return await asyncio.to_thread(run_action, req.action, req.id, **extra)

    @app.get("/tr808/state")
    async def tr808_state(id: Optional[str] = None) -> Dict[str, Any]:
        return await asyncio.to_thread(run_action, "state", id)

    @app.get("/tr808/relay")
    async def tr808_relay() -> Dict[str, Any]:
        r = state["relay"]
        return {"running": bool(r), "connected": bool(r and r.connected), "relayed": r.count if r else 0}

    async def _start() -> None:
        if os.environ.get("WWDOS_TR808_RELAY", "1") == "0":
            return
        r = Relay(bus, asyncio.get_running_loop())
        r.start()
        state["relay"] = r

    async def _stop() -> None:
        if state["relay"]:
            state["relay"].stop.set()

    import warnings
    with warnings.catch_warnings():   # FastAPI marks on_event deprecated; main.py has no lifespan
        warnings.simplefilter("ignore", DeprecationWarning)
        app.router.on_event("startup")(_start)
        app.router.on_event("shutdown")(_stop)
