#!/usr/bin/env python3
"""Contract tests for the TR-808 routes and event relay (tools/api_server/tr808.py).

Runs without the C++ app: IPC send_cmd is replaced by a recorder.
    tools/api_server/venv/bin/python -m unittest tests/test_tr808_api.py -v
"""
import asyncio
import json
import os
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
os.environ["WWDOS_TR808_RELAY"] = "0"  # no IPC event subscriber in contract tests

from fastapi import FastAPI  # noqa: E402
from fastapi.testclient import TestClient  # noqa: E402

import tools.api_server.tr808 as tr808_mod  # noqa: E402
from tools.api_server.show import ShowBus  # noqa: E402


class Tr808RouteTest(unittest.TestCase):
    def setUp(self):
        self.sent = []

        def fake_send(cmd, kv=None):
            self.sent.append((cmd, dict(kv or {})))
            if kv.get("action") == "bogus":
                return "err unknown action"
            return json.dumps({"id": "w4", "ok": True, "transport": "playing"})

        self._orig = tr808_mod.send_cmd
        tr808_mod.send_cmd = fake_send
        self.app = FastAPI()
        self.bus = ShowBus()
        tr808_mod.register(self.app, self.bus)

    def tearDown(self):
        tr808_mod.send_cmd = self._orig

    def test_action_forwards_args_as_exec_command(self):
        # No `with`: the startup hook (the relay thread) stays off.
        c = TestClient(self.app)
        r = c.post("/tr808", json={"action": "set_step", "instrument": "bd", "step": 4, "active": True, "id": "w4"})
        self.assertEqual(r.status_code, 200, r.text)
        self.assertEqual(r.json()["id"], "w4")
        cmd, kv = self.sent[-1]
        self.assertEqual(cmd, "exec_command")
        self.assertEqual(kv["name"], "tr808")
        self.assertEqual(kv["action"], "set_step")
        self.assertEqual(kv["id"], "w4")
        self.assertEqual(kv["step"], "4")
        self.assertEqual(kv["active"], "1")

    def test_app_error_is_400(self):
        c = TestClient(self.app)
        r = c.post("/tr808", json={"action": "bogus"})
        self.assertEqual(r.status_code, 400)

    def test_relay_puts_step_events_on_the_bus_without_frames(self):
        loop = asyncio.new_event_loop()
        try:
            relay = tr808_mod.Relay(self.bus, loop)
            payload = {"id": "w4", "step": 4, "voices": ["bd", "sd"], "accent": True, "tempo": 120, "at": 0}
            relay._line(json.dumps({"type": "subscribed"}).encode())
            relay._line(json.dumps({"type": "event", "seq": 9, "event": "tr808_step", "payload": payload}).encode())
            relay._line(json.dumps({"type": "event", "seq": 10, "event": "state_changed", "payload": {}}).encode())
            loop.run_until_complete(asyncio.sleep(0.01))
        finally:
            loop.close()
        self.assertTrue(relay.connected)
        self.assertEqual(relay.count, 1)
        ev = self.bus.ring[-1]
        self.assertEqual(ev["type"], "tr808_step")
        self.assertEqual(ev["window"]["id"], "w4")
        self.assertEqual(ev["data"]["voices"], ["bd", "sd"])
        self.assertEqual(ev["data"]["app_seq"], 9)


if __name__ == "__main__":
    unittest.main()
