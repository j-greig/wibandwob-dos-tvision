"""Helpers for the resident input tests: a private desktop instance, its socket, and a person.

Each Desk runs one wwdos instance in its own tmux server (tmux -L <name>) with its own
IPC socket (/tmp/wibwob_<name>.sock), so a test never touches another session's desktop.
The person's input goes in through tmux (keys, and xterm SGR mouse sequences), the way a
keyboard and mouse reach the app; the resident's goes through the socket.
"""

import json
import os
import shlex
import socket
import subprocess
import time
import urllib.parse

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "build-resident", "app", "wwdos")


class Desk:
    def __init__(self, name, binary=None, cols=160, rows=50, env=None):
        self.name = name
        # A private self on disk, no boot screen, no sleep and no restored windows unless a test asks, so a
        # desktop starts with no windows of its own and never writes to ~/.wwdos.
        self.home = f"/tmp/wibwob_{name}.home"
        self.env = {"WWDOS_HOME": self.home, "WWDOS_BOOT": "0", "WWDOS_SLEEP_MS": "0",
                    "WWDOS_RESTORE": "0", **(env or {})}
        self.binary = binary or os.environ.get("WWDOS_BIN", DEFAULT_BIN)
        self.cols, self.rows = cols, rows
        self.sock = f"/tmp/wibwob_{name}.sock"
        self.log = f"/tmp/wibwob_{name}.stderr"

    # ---------------------------------------------------------------- life
    def start(self, timeout=10.0):
        self.stop()
        extra = "".join(f"{k}={shlex.quote(v)} " for k, v in self.env.items())
        cmd = (f"cd {REPO} && {extra}WIBWOB_INSTANCE={self.name} WWDOS_TR808_AUDIO=0 "
               f"{self.binary} 2>{self.log}")
        subprocess.run(["tmux", "-L", self.name, "new-session", "-d", "-s", self.name,
                        "-x", str(self.cols), "-y", str(self.rows), cmd], check=True)
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                if self.ipc("get_canvas_size"):
                    return self
            except OSError:
                pass
            time.sleep(0.1)
        raise RuntimeError(f"desktop {self.name} did not start ({self.binary})")

    def quit(self, timeout=10.0):
        """The person chooses Exit (Alt+X); returns True when the app has ended."""
        self.person_keys("M-x")
        t0 = time.time()
        while time.time() - t0 < timeout:
            if not self.alive():
                return True
            time.sleep(0.1)
        return False

    def stop(self):
        subprocess.run(["tmux", "-L", self.name, "kill-server"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.2)

    def alive(self):
        r = subprocess.run(["tmux", "-L", self.name, "has-session", "-t", self.name],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return r.returncode == 0

    # ---------------------------------------------------------------- socket
    def ipc(self, cmd, timeout=5.0, **kv):
        parts = [f"cmd:{cmd}"] + [f"{k}={urllib.parse.quote(str(v), safe='')}" for k, v in kv.items()]
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(timeout)
        s.connect(self.sock)
        s.sendall((" ".join(parts) + "\n").encode())
        chunks = []
        while True:
            try:
                b = s.recv(65536)
            except socket.timeout:
                break
            if not b:
                break
            chunks.append(b)
        s.close()
        return b"".join(chunks).decode("utf-8", "replace").strip()

    def j(self, cmd, **kv):
        r = self.ipc(cmd, **kv)
        try:
            return json.loads(r)
        except json.JSONDecodeError:
            raise AssertionError(f"{cmd} {kv} -> {r!r}")

    def state(self):
        return self.j("get_state")

    def windows(self):
        return {w["id"]: w for w in self.state()["windows"]}

    def focused(self):
        for w in self.state()["windows"]:
            if w.get("focused"):
                return w["id"]
        return None

    def editor(self, x, y, w, h, title="ed"):
        r = self.j("create_window", type="text_editor", x=x, y=y, w=w, h=h, title=title)
        return r["id"]

    def text(self, wid):
        r = self.ipc("window_text", id=wid)
        if r == "err unknown cmd":
            return self.text_on_screen(wid)   # an app without window_text (the unmodified build)
        return json.loads(r)["lines"]

    def text_on_screen(self, wid):
        """The window's text rows as the person sees them (no scrolled-off text)."""
        w = self.windows()[wid]
        rows = self.screen()
        out = []
        for y in range(w["y"] + 2, w["y"] + w["h"]):
            line = rows[y] if y < len(rows) else ""
            out.append(line[w["x"] + 1:w["x"] + w["w"] - 2].rstrip())
        while out and not out[-1]:
            out.pop()
        return out or [""]

    def wait_seqs(self, first, last, timeout=15.0):
        """Wait until every resident item first..last has a result in the log; return them by seq."""
        want = set(range(first, last + 1))
        got = {}
        t0 = time.time()
        while time.time() - t0 < timeout:
            st = self.j("resident_status", since=first - 1, limit=4000)
            for e in st["log"]:
                if e["seq"] in want:
                    got[e["seq"]] = e
            if want <= set(got):
                return got
            time.sleep(0.05)
        raise AssertionError(f"resident items {sorted(want - set(got))[:10]}... not done in {timeout}s")

    # ---------------------------------------------------------------- the person
    def person_text(self, text):
        subprocess.run(["tmux", "-L", self.name, "send-keys", "-t", self.name, "-l", text], check=True)

    def person_type(self, text, gap=0.012):
        """Type like a person: one key per write. Three or more printable keys in one read are
        a paste to Turbo Vision (kbPaste), which the text editor view does not insert."""
        for ch in text:
            self.person_text(ch)
            time.sleep(gap)

    def person_keys(self, *keys):
        subprocess.run(["tmux", "-L", self.name, "send-keys", "-t", self.name, *keys], check=True)

    def person_mouse(self, kind, x, y, button=0):
        """kind: down | up | drag. x, y are screen cells (0-based); sent as xterm SGR (1006)."""
        code = {"down": button, "up": button, "drag": 32 + button}[kind]
        end = "m" if kind == "up" else "M"
        self.person_text(f"\x1b[<{code};{x + 1};{y + 1}{end}")

    def screen(self):
        r = subprocess.run(["tmux", "-L", self.name, "capture-pane", "-p", "-N", "-t", self.name],
                           capture_output=True, text=True)
        return r.stdout.split("\n")


class Results:
    def __init__(self):
        self.passed = 0
        self.failed = 0

    def check(self, cond, msg):
        if cond:
            self.passed += 1
            print(f"  PASS: {msg}")
        else:
            self.failed += 1
            print(f"  FAIL: {msg}")
        return cond
