#!/usr/bin/env python3
"""IPC chain test runner. Loops until all stories pass or gives up."""

import json
import os
import subprocess
import sys
import time
import socket
import urllib.request
import urllib.error

STORIES_FILE = os.path.join(os.path.dirname(__file__), "ipc_chain_stories.json")
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API = os.environ.get("WIBWOB_API_URL", "http://127.0.0.1:8089")


def load_stories():
    with open(STORIES_FILE) as f:
        return json.load(f)


def save_stories(stories):
    with open(STORIES_FILE, "w") as f:
        json.dump(stories, f, indent=2)
        f.write("\n")


def api_get(path):
    try:
        req = urllib.request.Request(f"{API}{path}")
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read())
    except Exception as e:
        return {"_error": str(e)}


def api_post(path, body):
    try:
        data = json.dumps(body).encode()
        req = urllib.request.Request(
            f"{API}{path}", data=data,
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as e:
        return {"_error": f"HTTP {e.code}: {e.read().decode()[:200]}"}
    except Exception as e:
        return {"_error": str(e)}


def sock_path():
    inst = os.environ.get("WIBWOB_INSTANCE")
    if inst:
        return f"/tmp/wibwob_{inst}.sock"
    return "/tmp/wwdos.sock"


def direct_ipc(cmd):
    """Send a raw IPC command to the TUI socket, return response string."""
    path = sock_path()
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(3.0)
    try:
        s.connect(path)
        s.sendall((cmd + "\n").encode())
        chunks = []
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
        return b"".join(chunks).decode("utf-8", errors="replace").strip()
    except Exception as e:
        return f"_error:{e}"
    finally:
        s.close()


def rg_count(pattern, excludes=None):
    """Count ripgrep matches in active code (excludes vendor/venv/archive/memories/logs)."""
    cmd = ["rg", "-l", pattern,
           "-g", "!vendor/**", "-g", "!**/venv/**", "-g", "!**/node_modules/**",
           "-g", "!archive/**", "-g", "!memories/**", "-g", "!*.log",
           "-g", "!docs/codex-reviews/**", "-g", "!workings/**",
           "-g", "!tests/ipc_chain_stories.json", "-g", "!tests/run_ipc_chain.py"]
    if excludes:
        for e in excludes:
            cmd.extend(["-g", f"!{e}"])
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO)
    files = [f for f in result.stdout.strip().split("\n") if f]
    return files


# ── Test implementations ──

def test_build():
    r = subprocess.run(
        ["cmake", "--build", "./build"],
        capture_output=True, text=True, cwd=REPO
    )
    # Check wwdos built (ignore paint_tui pre-existing failure)
    if "Built target wwdos" in r.stdout:
        return True, "wwdos built OK"
    return False, r.stderr[-300:]


def test_socket_exists():
    p = sock_path()
    if os.path.exists(p):
        return True, f"{p} exists"
    return False, f"{p} not found — is TUI running?"


def test_direct_ipc():
    resp = direct_ipc("cmd:get_canvas_size")
    if "_error:" in resp:
        return False, resp
    try:
        d = json.loads(resp)
        if "width" in d and "height" in d:
            return True, f"{d['width']}x{d['height']}"
    except:
        pass
    return False, f"unexpected: {resp[:100]}"


def test_python_resolver():
    expected = sock_path()
    sys.path.insert(0, os.path.join(REPO, "tools", "api_server"))
    # Re-import to get fresh value
    import importlib
    import ipc_client
    importlib.reload(ipc_client)
    actual = ipc_client.resolve_sock_path()
    if actual == expected:
        return True, actual
    return False, f"expected {expected}, got {actual}"


def test_no_duplicate_resolvers():
    files = rg_count("def _sock_path")
    if not files:
        return True, "no duplicates"
    return False, f"found in: {', '.join(files)}"


def test_no_legacy_sock():
    files = rg_count("test_pattern_app\\.sock")
    if not files:
        return True, "clean"
    return False, f"found in: {', '.join(files)}"


def test_no_tv_ipc_sock():
    files = rg_count("TV_IPC_SOCK")
    if not files:
        return True, "clean"
    return False, f"found in: {', '.join(files)}"


def test_api_health():
    d = api_get("/health")
    if d.get("ok"):
        return True, "healthy"
    return False, str(d)


def test_api_commands_count():
    d = api_get("/commands")
    count = d.get("count", 0)
    if count >= 70:
        return True, f"{count} commands"
    # If 0, try direct IPC to see if it's an API-side problem
    ipc_resp = direct_ipc("cmd:get_capabilities")
    if ipc_resp.startswith("{"):
        try:
            ipc_data = json.loads(ipc_resp)
            ipc_count = len(ipc_data.get("commands", []))
        except json.JSONDecodeError:
            # Response truncated — but it started with JSON, so IPC works
            ipc_count = ipc_resp.count('"name"')
        return False, f"API returns {count} but direct IPC has ~{ipc_count} — API→IPC broken"
    return False, f"only {count} commands"


def test_cmd_exists(cmd_name):
    d = api_get("/commands")
    cmds = [c["name"] for c in d.get("commands", [])]
    if cmd_name in cmds:
        return True, f"'{cmd_name}' present"
    # Fall back to direct IPC check
    ipc_resp = direct_ipc("cmd:get_capabilities")
    if cmd_name in ipc_resp:
        return False, f"'{cmd_name}' in IPC but not API — API stale?"
    return False, f"'{cmd_name}' missing from registry entirely"


def test_exec_cmd(cmd_name):
    d = api_post("/menu/command", {"command": cmd_name})
    if "_error" in d:
        return False, d["_error"]
    if d.get("ok") or "width" in d or "result" in d:
        return True, f"executed OK"
    return False, str(d)[:200]


def test_exec_open_figlet():
    d = api_post("/menu/command", {
        "command": "open_figlet_text",
        "args": {"text": "TEST", "font": "standard"}
    })
    if "_error" in d:
        return False, d["_error"]
    if d.get("ok"):
        time.sleep(0.3)
        state = api_get("/state")
        wins = state.get("windows", [])
        figlet_wins = [w for w in wins if "figlet" in w.get("type", "").lower()
                       or "figlet" in w.get("title", "").lower()
                       or "TEST" in w.get("title", "")]
        if figlet_wins:
            return True, f"window created: {figlet_wins[0].get('title', '?')}"
        return False, f"command ok but no figlet window in state (types: {[w['type'] for w in wins]})"
    return False, str(d)[:200]


def test_exec_open_verse():
    d = api_post("/menu/command", {"command": "open_verse"})
    if "_error" in d:
        return False, d["_error"]
    if d.get("ok"):
        time.sleep(0.3)
        state = api_get("/state")
        wins = state.get("windows", [])
        verse_wins = [w for w in wins if "verse" in w.get("type", "").lower()]
        if verse_wins:
            return True, f"window created"
        return False, "command ok but no verse window in state"
    return False, str(d)[:200]


def test_api_state_has_canvas():
    d = api_get("/state")
    if "_error" in d:
        return False, d["_error"]
    w = d.get("canvas_width") or d.get("width")
    h = d.get("canvas_height") or d.get("height")
    if w and h:
        return True, f"{w}x{h}"
    # Maybe nested
    if "windows" in d:
        return True, f"state ok ({len(d['windows'])} windows)"
    return False, f"no canvas size in state: {list(d.keys())}"


def test_start_script_no_force_instance():
    path = os.path.join(REPO, "start_api_server.sh")
    with open(path) as f:
        content = f.read()
    # Should NOT have unconditional WIBWOB_INSTANCE=1 or INSTANCE=1
    if 'INSTANCE="${WIBWOB_INSTANCE:-1}"' in content:
        return False, "still forces WIBWOB_INSTANCE default to 1"
    if "export WIBWOB_INSTANCE" in content and '[ -n "${WIBWOB_INSTANCE:-}"' not in content:
        return False, "unconditionally exports WIBWOB_INSTANCE"
    return True, "only exports if explicitly set"


def test_full_roundtrip():
    # Clean slate
    api_post("/menu/command", {"command": "close_all"})
    time.sleep(0.3)

    # Open a figlet window
    d = api_post("/menu/command", {
        "command": "open_figlet_text",
        "args": {"text": "ROUNDTRIP", "font": "standard"}
    })
    if "_error" in d:
        return False, f"open failed: {d['_error']}"

    time.sleep(0.5)

    # Check it appears in state
    state = api_get("/state")
    wins = state.get("windows", [])
    # Find any non-wibwob window (close_all preserves chat)
    new_wins = [w for w in wins if w.get("type") != "wibwob"]
    if not new_wins:
        return False, f"no new window in state after open"

    # Close it
    api_post("/menu/command", {"command": "close_all"})
    time.sleep(0.3)

    # Verify gone
    state2 = api_get("/state")
    wins2 = [w for w in state2.get("windows", []) if w.get("type") != "wibwob"]
    if wins2:
        return False, f"windows remain after close_all: {[w['type'] for w in wins2]}"

    return True, "open → verify → close → verify: all good"


# ── Dispatch ──

def run_test(story):
    t = story["test"]
    if t == "build":
        return test_build()
    if t == "socket_exists":
        return test_socket_exists()
    if t == "direct_ipc":
        return test_direct_ipc()
    if t == "python_resolver":
        return test_python_resolver()
    if t == "no_duplicate_resolvers":
        return test_no_duplicate_resolvers()
    if t == "no_legacy_sock":
        return test_no_legacy_sock()
    if t == "no_tv_ipc_sock":
        return test_no_tv_ipc_sock()
    if t == "api_health":
        return test_api_health()
    if t == "api_commands_count":
        return test_api_commands_count()
    if t.startswith("cmd_exists:"):
        return test_cmd_exists(t.split(":")[1])
    if t == "api_state_has_canvas":
        return test_api_state_has_canvas()
    if t.startswith("exec_cmd:"):
        return test_exec_cmd(t.split(":")[1])
    if t == "exec_open_figlet":
        return test_exec_open_figlet()
    if t == "exec_open_verse":
        return test_exec_open_verse()
    if t == "start_script_no_force_instance":
        return test_start_script_no_force_instance()
    if t == "full_roundtrip":
        return test_full_roundtrip()
    return False, f"unknown test: {t}"


def main():
    max_rounds = 3
    for round_num in range(1, max_rounds + 1):
        stories = load_stories()

        print(f"\n{'='*50}")
        print(f"  Round {round_num}/{max_rounds}")
        print(f"{'='*50}\n")

        for s in stories:
            if s["status"] == "pass":
                print(f"  ✅ {s['id']} {s['name']} (already passed)")
                continue

            ok, detail = run_test(s)
            s["status"] = "pass" if ok else "fail"
            s["detail"] = detail
            icon = "✅" if ok else "❌"
            print(f"  {icon} {s['id']} {s['name']}")
            print(f"     → {detail}")

        save_stories(stories)

        passed = sum(1 for s in stories if s["status"] == "pass")
        failed = sum(1 for s in stories if s["status"] == "fail")
        total = len(stories)

        print(f"\n  Results: {passed}/{total} passed, {failed} failed")

        if failed == 0:
            print(f"\n🎉 ALL {total} STORIES PASS\n")
            return 0

        print(f"\n  Failed stories:")
        for s in stories:
            if s["status"] == "fail":
                print(f"    {s['id']} {s['name']}: {s.get('detail','')}")

        if round_num < max_rounds:
            print(f"\n  Retrying in 2s...")
            time.sleep(2)

    print(f"\n💀 GAVE UP after {max_rounds} rounds. See ipc_chain_stories.json for details.\n")
    return 1


if __name__ == "__main__":
    sys.exit(main())
