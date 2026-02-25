#!/usr/bin/env python3
"""
Tests for F04: bridge exits cleanly when TUI's IPC socket disappears.

Verifies:
  1. _check_socket_alive raises TUIExited when socket file is gone
  2. _check_socket_alive is a no-op when socket file exists
  3. poll_loop raises TUIExited (propagating to run()) when socket disappears
  4. event_subscribe_loop raises TUIExited when socket disappears mid-retry
"""
from __future__ import annotations

import asyncio
import os
import socket
import sys
import tempfile
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools" / "room"))

from partykit_bridge import TUIExited, _check_socket_alive, PartyKitBridge  # noqa: E402

# ── helpers ───────────────────────────────────────────────────────────────────

def _make_unix_sock(path: str) -> None:
    """Create a real Unix domain socket file."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        s.bind(path)
    finally:
        s.close()


# ── test 1: _check_socket_alive ───────────────────────────────────────────────

def test_check_socket_alive_raises_when_gone():
    """TUIExited raised if socket file does not exist."""
    path = "/tmp/wibwob_test_gone_99999.sock"
    if os.path.exists(path):
        os.unlink(path)
    try:
        _check_socket_alive(path)
        assert False, "Expected TUIExited"
    except TUIExited as e:
        assert "99999" in str(e) or "gone" in str(e).lower()
    print("  ✓ _check_socket_alive raises TUIExited when socket missing")


def test_check_socket_alive_ok_when_present():
    """No exception raised when socket file exists."""
    with tempfile.NamedTemporaryFile(suffix=".sock", delete=False) as f:
        path = f.name
    try:
        _check_socket_alive(path)   # should not raise
    finally:
        os.unlink(path)
    print("  ✓ _check_socket_alive silent when socket present")


# ── test 2: poll_loop exits via TUIExited ─────────────────────────────────────

def test_poll_loop_raises_tui_exited_when_socket_gone():
    """
    poll_loop calls _sync_state_now which returns None when IPC fails.
    If the socket is also gone, _check_socket_alive should raise TUIExited,
    which propagates out of poll_loop.
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        sock_path = os.path.join(tmpdir, "wibwob_test.sock")

        bridge = PartyKitBridge.__new__(PartyKitBridge)
        bridge.sock_path = sock_path
        bridge.instance_id = "test"
        bridge.legacy_scramble_chat = True   # skip chat drain
        bridge._state_lock = None
        bridge.last_windows = {}
        bridge._self_conn_id = None
        bridge._participants = []

        async def run():
            # _sync_state_now → returns None (socket not present)
            # _check_socket_alive → raises TUIExited (socket not present)
            with patch.object(bridge, "_sync_state_now", new_callable=AsyncMock,
                              return_value=None):
                await bridge.poll_loop()

        try:
            asyncio.run(asyncio.wait_for(run(), timeout=2.0))
            assert False, "Expected TUIExited"
        except TUIExited:
            pass
        except asyncio.TimeoutError:
            assert False, "poll_loop did not raise TUIExited — it hung (zombie behaviour)"

    print("  ✓ poll_loop raises TUIExited when socket disappears")


# ── test 3: event_subscribe_loop exits via TUIExited ─────────────────────────

def test_event_subscribe_loop_raises_tui_exited_when_socket_gone():
    """
    event_subscribe_loop catches OSError on socket connect.
    If socket file is gone → _check_socket_alive raises TUIExited.
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        sock_path = os.path.join(tmpdir, "wibwob_evt_test.sock")

        bridge = PartyKitBridge.__new__(PartyKitBridge)
        bridge.sock_path = sock_path
        bridge.instance_id = "test"

        # Patch log to be silent
        bridge.log = lambda msg: None

        async def run():
            await bridge.event_subscribe_loop()

        try:
            asyncio.run(asyncio.wait_for(run(), timeout=2.0))
            assert False, "Expected TUIExited"
        except TUIExited:
            pass
        except asyncio.TimeoutError:
            assert False, "event_subscribe_loop did not exit — zombie behaviour"

    print("  ✓ event_subscribe_loop raises TUIExited when socket disappears")


# ── test 4: integration — socket present then deleted ─────────────────────────

def test_poll_loop_exits_after_socket_deleted():
    """
    Socket exists initially (so bridge starts), then is deleted.
    poll_loop should raise TUIExited on next iteration.
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        sock_path = os.path.join(tmpdir, "wibwob_live_test.sock")

        # Create a real socket file
        _make_unix_sock(sock_path)
        assert os.path.exists(sock_path)

        call_count = [0]

        bridge = PartyKitBridge.__new__(PartyKitBridge)
        bridge.sock_path = sock_path
        bridge.instance_id = "test"
        bridge.legacy_scramble_chat = True
        bridge._state_lock = None
        bridge.last_windows = {}
        bridge._self_conn_id = None
        bridge._participants = []

        async def fake_sync(trigger="poll"):
            call_count[0] += 1
            if call_count[0] == 1:
                return {"windows": []}   # first call: TUI alive, state ok
            # second call: TUI died, delete socket and return None
            if os.path.exists(sock_path):
                os.unlink(sock_path)
            return None

        async def run():
            with patch.object(bridge, "_sync_state_now", side_effect=fake_sync), \
                 patch("partykit_bridge.POLL_INTERVAL", 0.01):
                await bridge.poll_loop()

        try:
            asyncio.run(asyncio.wait_for(run(), timeout=3.0))
            assert False, "Expected TUIExited"
        except TUIExited:
            assert call_count[0] >= 2, "Expected at least 2 sync calls"
        except asyncio.TimeoutError:
            assert False, "poll_loop hung after socket deleted"

    print("  ✓ poll_loop exits cleanly after socket file deleted mid-run")


# ── runner ────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("F04 — bridge exit fix tests")
    print()
    tests = [
        test_check_socket_alive_raises_when_gone,
        test_check_socket_alive_ok_when_present,
        test_poll_loop_raises_tui_exited_when_socket_gone,
        test_event_subscribe_loop_raises_tui_exited_when_socket_gone,
        test_poll_loop_exits_after_socket_deleted,
    ]
    failed = []
    for t in tests:
        try:
            t()
        except Exception as e:
            print(f"  ✗ {t.__name__}: {e}")
            failed.append(t.__name__)
    print()
    if failed:
        print(f"FAIL — {len(failed)}/{len(tests)} tests failed: {failed}")
        sys.exit(1)
    else:
        print(f"PASS — {len(tests)}/{len(tests)} tests passed")
