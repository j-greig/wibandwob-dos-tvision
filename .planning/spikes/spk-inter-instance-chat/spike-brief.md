---
type: spike
status: partially-done
tags: [multi-agent, infrastructure, ipc]
tldr: "Inter-instance group chat via IPC: injection exists; history export + broker fanout remain"
updated: 2026-02-24
---

# Spike: Inter-Instance Group Chat (Human + Multiple AIs)

## Simple Description (Non-Technical)

This spike is a **group chat**, not a point-to-point relay.

Think WhatsApp with 3 participants:
- Human
- AI in WibWob instance A
- AI in WibWob instance B

When **anyone** sends a message, **everyone else sees it**. The human can type into any instance at any time, and both AIs can continue responding.
The human's messages appear in all instances too, not just the AI responses.

## Research Question

**Can multiple WibWob-DOS instances participate in one shared group chat, with the human able to inject into any instance mid-conversation?**

## Timebox

Remaining: ~2-4 hours depending on whether safety controls are included in the first pass.

## What Already Exists (discovered 2026-02-24)

The original spike assumed nothing existed. In fact, most of Phase 1 is already shipped.

### ✅ `wibwob_ask` IPC command (already shipped)

- File: `app/command_registry.cpp:96`
- Usage: `cmd:exec_command name=wibwob_ask text=hello`
- Behavior: injects a user message into the Wib&Wob chat and triggers an LLM response.

### ✅ `chat_receive` IPC command (already shipped)

- File: `app/command_registry.cpp:95`
- Behavior: displays a remote message in the chat UI without triggering an LLM response.
- Useful for display-only fanout / future broker modes.

### ✅ `TWibWobWindow::injectUserMessage()` (already shipped)

- File: `app/wibwob_view.h:153`
- Public method used to queue IPC-injected prompts into the chat loop safely.
- Drained asynchronously via the idle loop.

### ✅ Async injection path already exists (important)

- Event-posted custom event path in `app/test_pattern_app.cpp:3061`
- `pendingAsk_` drain loop in `app/wibwob_view.cpp:516`
- This means IPC callers do not block on model generation.

## What Needs Building

## 1. `get_chat_history` (C++)

A new IPC command that returns the current Wib&Wob chat transcript as JSON.

Required output shape:

```json
{"messages":[{"role":"user","content":"hello"},{"role":"assistant","content":"hi"}]}
```

Key requirement: it must capture **streaming assistant replies**, not just non-streaming paths.

## 2. Broker script (Python)

A local coordinator process that:
- discovers live WibWob Unix sockets (`/tmp/wibwob_*.sock`)
- polls `get_chat_history`
- detects new messages
- broadcasts **all messages (user and assistant)** to all other instances
- preserves attribution (which instance + role)
- deduplicates / suppresses broker echoes
- applies safety controls (max turns, cooldown, token budget)

## Important Reframe: Group Chat vs Relay

The earlier relay framing only forwarded assistant replies from one instance to another. That is not the target model.

Target model is shared conversation fanout:
- Human speaks in instance A -> broker delivers that message to instances B/C
- Assistant in instance B replies -> broker delivers that reply to instances A/C
- Assistant in instance C replies -> broker delivers that reply to instances A/B
- Everyone sees everything

In practice, the broker can use `wibwob_ask` for fanout so remote AIs actually "hear" the message and can respond, with attribution embedded in the prompt text. `chat_receive` remains available for display-only fanout modes.

## Transport

Local WibWob-DOS IPC via Unix sockets (`/tmp/wibwob_*.sock`), coordinated by a Python broker.

```text
Instance A  <--IPC-->  Broker  <--IPC-->  Instance B
     ^                                   \
     |                                    <--IPC-->  Instance C (optional)
     +--------------------------------------------------------------+
                     all messages broadcast to all peers
```

## Preflight Checklist (before code changes)

Run these before editing application code:

1. Confirm Phase 1 commands still exist:
   - `rg -n "wibwob_ask|chat_receive" app/command_registry.cpp app/test_pattern_app.cpp`
2. Confirm async injection event path still exists:
   - `rg -n "0xF0F0|pendingAsk_|putEvent" app/test_pattern_app.cpp app/wibwob_view.cpp`
3. Confirm `TWibWobWindow::injectUserMessage` is public:
   - `rg -n "injectUserMessage" app/wibwob_view.h`
4. Confirm `TWibWobMessageView` still owns message storage and streaming methods:
   - `rg -n "class TWibWobMessageView|addMessage|startStreamingMessage|finishStreamingMessage" app/wibwob_view.h app/wibwob_view.cpp`
5. Confirm IPC command parser still percent-decodes generic `k=v` params:
   - `rg -n "percent_decode|kv\\[tok.substr\\(0, eq\\)\\]" app/api_ipc.cpp`
6. Confirm reusable Python patterns still exist:
   - `tools/monitor/instance_monitor.py`
   - `tools/api_server/ipc_client.py`

If any moved, re-run `rg` and adjust the line references below before patching.

## What Is Already Done (Do Not Re-implement)

- `wibwob_ask` command registration + dispatch in `app/command_registry.cpp`
- `api_wibwob_ask(...)` IPC bridge in `app/test_pattern_app.cpp`
- `TWibWobWindow::injectUserMessage(...)` public method in `app/wibwob_view.h`
- Async event-posted injection handling in `TWibWobWindow::handleEvent(...)`

## Critical Gotcha: Streaming Replies Bypass `addMessage()`

The earlier brief note ("append in `addMessage()`") is not sufficient.

- `addMessage()` is used for user messages and some non-stream/error paths.
- Streaming assistant replies use:
  - `TWibWobMessageView::startStreamingMessage(...)` at `app/wibwob_view.cpp:132`
  - `appendToStreamingMessage(...)` at `app/wibwob_view.cpp:160`
  - `finishStreamingMessage(...)` at `app/wibwob_view.cpp:174`

If history is only recorded in `addMessage()`, `get_chat_history` will miss streamed assistant replies.

## Source Recon (Exact Insertion Points)

### 1. `TWibWobMessageView` additions (history buffer + JSON export)

File: `app/wibwob_view.h`

- Class starts at `app/wibwob_view.h:46`
- Existing message API methods at `app/wibwob_view.h:54` (`addMessage`) and `:55` (`clear`)
- Existing getter at `app/wibwob_view.h:70`
- Private fields block starts at `app/wibwob_view.h:72`
- Existing message vectors at `app/wibwob_view.h:79-80`

File: `app/wibwob_view.cpp`

- `TWibWobMessageView::addMessage(...)` at `app/wibwob_view.cpp:73`
- `TWibWobMessageView::clear()` at `app/wibwob_view.cpp:92`
- `startStreamingMessage(...)` at `app/wibwob_view.cpp:132`
- `appendToStreamingMessage(...)` at `app/wibwob_view.cpp:160`
- `finishStreamingMessage(...)` at `app/wibwob_view.cpp:174`
- `cancelStreamingMessage(...)` at `app/wibwob_view.cpp:187`

### 2. Command registration + dispatch (`get_chat_history`)

File: `app/command_registry.cpp`

- `extern` declarations near top at `app/command_registry.cpp:15-57`
  - Add `extern std::string api_get_chat_history(TTestPatternApp& app);` near `api_wibwob_ask` (`:20-22`)
- Capabilities table at `app/command_registry.cpp:66-115`
  - Add a row near `chat_receive` / `wibwob_ask` (`:95-97`)
- `exec_registry_command(...)` begins at `app/command_registry.cpp:152`
- `chat_receive` / `wibwob_ask` dispatch branches at `app/command_registry.cpp:312-325`
  - Add `if (name == "get_chat_history") ...` adjacent to these

### 3. IPC bridge implementation pattern (find `TWibWobWindow`)

File: `app/test_pattern_app.cpp`

- `api_chat_receive(...)` at `app/test_pattern_app.cpp:3026`
- `api_wibwob_ask(...)` at `app/test_pattern_app.cpp:3035`
- Window search + async event pattern inside `api_wibwob_ask(...)` at `app/test_pattern_app.cpp:3039-3071`

Reuse the desktop scan + `dynamic_cast<TWibWobWindow*>` portion for `api_get_chat_history(...)`.

Do not reuse the event-posting part; `get_chat_history` is read-only and should return synchronously.

### 4. Socket discovery + IPC format reuse for broker

File: `tools/monitor/instance_monitor.py`

- `discover_sockets()` at `tools/monitor/instance_monitor.py:19-25`

File: `tools/api_server/ipc_client.py`

- `send_cmd(...)` at `tools/api_server/ipc_client.py:26-106`
- Command wire format built at `tools/api_server/ipc_client.py:56-69`
- Encoding behavior (`content` base64 special-case only) at `tools/api_server/ipc_client.py:58-67`

File: `app/api_ipc.cpp` (encoding behavior verification)

- Generic percent decode helper at `app/api_ipc.cpp:28-51`
- `k=v` parse path (applies to `exec_command` params) at `app/api_ipc.cpp:379-392`
- Base64 decode special-case exists for `send_text content=...` only at `app/api_ipc.cpp:505-517`

## Exact Additions to Implement (C++)

### A. `app/wibwob_view.h` exact signatures / structs to add

Add these inside `class TWibWobMessageView`.

Public section (near `addMessage` / `clear`, around `app/wibwob_view.h:54-56`):

```cpp
std::string getHistoryJson() const;
```

Private section (after `WrappedLine`, before existing vectors around `app/wibwob_view.h:79`):

```cpp
struct HistoryEntry {
    std::string role;     // "user" | "assistant" | "system"
    std::string content;
};
```

Private fields (near `messages` / `wrappedLines`):

```cpp
std::vector<HistoryEntry> chatHistory_;
```

Private helpers (below existing private fields/method declarations, before `rebuildWrappedLines()` or just above it):

```cpp
void recordHistoryMessage(const std::string& sender, const std::string& content, bool is_error);
static std::string mapSenderToRole(const std::string& sender, bool is_error);
```

### B. `app/wibwob_view.cpp` implementation details (exact behavior)

Implement these additions.

1. Add local JSON string escaper helper near top of file (after includes, before first method impl).

Required signature:

```cpp
static std::string jsonEscapeString(const std::string& s);
```

2. Implement history helpers:

```cpp
std::string TWibWobMessageView::mapSenderToRole(const std::string& sender, bool is_error);
void TWibWobMessageView::recordHistoryMessage(const std::string& sender, const std::string& content, bool is_error);
std::string TWibWobMessageView::getHistoryJson() const;
```

3. Hook history recording in these methods:

- `addMessage(...)` (`app/wibwob_view.cpp:73`): append to `chatHistory_` via `recordHistoryMessage(...)`
- `clear()` (`app/wibwob_view.cpp:92`): `chatHistory_.clear();`

4. Handle streaming assistant replies (required for correctness):

- In `startStreamingMessage(...)` (`app/wibwob_view.cpp:132`), append a placeholder history entry representing the streaming assistant message (`role="assistant"`, empty content)
- In `appendToStreamingMessage(...)` (`app/wibwob_view.cpp:160`), mirror appended chunk content into `chatHistory_.back().content` when it represents the current stream
- In `finishStreamingMessage(...)` (`app/wibwob_view.cpp:174`), no-op or final sync from `messages[streamingMessageIndex].content` into `chatHistory_.back().content`
- In `cancelStreamingMessage(...)` (`app/wibwob_view.cpp:187`), remove the placeholder history entry if the stream was canceled before completion

### C. Role mapping rules (use exactly)

Use these rules in `mapSenderToRole(...)`:

- `is_error == true` -> `"system"`
- sender case-insensitive `"User"` -> `"user"`
- sender empty string (`""`) -> `"assistant"` (non-streaming fallback path uses empty sender for Wib output at `app/wibwob_view.cpp:798`)
- sender case-insensitive `"Wib"` or `"Wib&Wob"` -> `"assistant"`
- sender case-insensitive `"System"` -> `"system"`
- otherwise default `"assistant"` (relay attribution prefixes are in content)

### D. JSON format returned by `TWibWobMessageView::getHistoryJson()`

Return a JSON array string only:

```json
[{"role":"user","content":"hello"},{"role":"assistant","content":"hi"}]
```

Do not wrap in `{"messages":...}` here; wrap at the IPC API layer.

### E. `app/test_pattern_app.cpp` new IPC bridge function

Add this function near `api_chat_receive(...)` / `api_wibwob_ask(...)` (same region around `app/test_pattern_app.cpp:3026-3072`):

```cpp
std::string api_get_chat_history(TTestPatternApp& app)
```

Implementation behavior:

1. Scan `app.deskTop` exactly like `api_wibwob_ask(...)` to find the first `TWibWobWindow`
2. Errors:
   - no chat window -> `err no wibwob chat window open`
   - no message view -> `err no wibwob message view`
3. Success return:
   - `std::string("{\"messages\":") + msgView->getHistoryJson() + "}"`

No event posting; this is synchronous and read-only.

### F. `app/command_registry.cpp` additions (copy existing pattern)

1. Add extern declaration near `api_wibwob_ask`:

```cpp
extern std::string api_get_chat_history(TTestPatternApp& app);
```

2. Add capabilities row next to chat commands:

```cpp
{"get_chat_history", "Return Wib&Wob chat history as JSON (messages: role+content)", false},
```

3. Add dispatch branch adjacent to `chat_receive` / `wibwob_ask`:

```cpp
if (name == "get_chat_history") {
    return api_get_chat_history(app);
}
```

## Full Broker Script (Python, Group Chat Fanout with Attribution + Dedup)

Create file: `tools/monitor/chat_coordinator.py`

This version is the **group chat** broker, not the older assistant-only relay. It broadcasts both user and assistant messages, includes attribution, and suppresses rebroadcast of broker-injected messages.

```python
#!/usr/bin/env python3
"""Inter-instance WibWob group chat broker via local Unix sockets.

Group-chat behavior:
- Poll each instance's chat history via get_chat_history
- Detect new messages (user + assistant)
- Broadcast each message to all other instances with attribution
- Suppress rebroadcast of broker-injected copies via side-channel dedup tracking
"""

from __future__ import annotations

import argparse
import glob
import hashlib
import json
import math
import os
import socket
import time
from typing import Dict, List, Set, Tuple

def discover_sockets() -> List[str]:
    """Find all /tmp/wibwob_*.sock files plus legacy path."""
    socks = sorted(glob.glob("/tmp/wibwob_*.sock"))
    legacy = "/tmp/test_pattern_app.sock"
    if os.path.exists(legacy) and legacy not in socks:
        socks.insert(0, legacy)
    return socks


def instance_name(sock_path: str) -> str:
    return os.path.basename(sock_path).replace(".sock", "")


def pct_encode(value: str) -> str:
    # Match C++ percent_decode() support in app/api_ipc.cpp.
    return (
        value.replace("%", "%25")
        .replace(" ", "%20")
        .replace("\n", "%0A")
        .replace("\r", "%0D")
        .replace("\t", "%09")
    )


def send_ipc_line(sock_path: str, line: str, timeout: float = 5.0) -> str:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect(sock_path)
    try:
        s.sendall((line.rstrip("\n") + "\n").encode("utf-8"))
        chunks: List[bytes] = []
        while True:
            chunk = s.recv(8192)
            if not chunk:
                break
            chunks.append(chunk)
        return b"".join(chunks).decode("utf-8", errors="ignore").strip()
    finally:
        s.close()


def exec_command(sock_path: str, name: str, **kv: str) -> str:
    parts = ["cmd:exec_command", f"name={name}"]
    for k, v in kv.items():
        parts.append(f"{k}={pct_encode(str(v))}")
    return send_ipc_line(sock_path, " ".join(parts))


def get_chat_history(sock_path: str) -> List[dict]:
    raw = exec_command(sock_path, "get_chat_history")
    if raw.startswith("err"):
        raise RuntimeError(raw)
    data = json.loads(raw)
    msgs = data.get("messages", [])
    if not isinstance(msgs, list):
        raise RuntimeError("invalid get_chat_history payload: messages is not a list")
    return msgs


def estimate_tokens(text: str) -> int:
    # Cheap budget proxy (4 chars/token heuristic)
    return max(1, math.ceil(len(text) / 4))


def pretty_instance_label(sock_path: str) -> str:
    name = instance_name(sock_path)
    if name.startswith("wibwob_"):
        suffix = name.split("_", 1)[1]
        if suffix.isdigit():
            return f"Instance {suffix}"
    return name


def is_relay_message(target_sock: str, content: str, delivered_to_target: Set[Tuple[str, str]]) -> bool:
    return (target_sock, content) in delivered_to_target


def make_event_id(source_label: str, role: str, index: int, content: str) -> str:
    # Dedup scope: source instance + role + transcript index + content
    # Good enough to suppress poll duplicates and broker-echo fanout loops.
    payload = f"{source_label}|{role}|{index}|{content}".encode("utf-8", errors="ignore")
    return hashlib.sha1(payload).hexdigest()[:12]


def format_relay_prompt(source_label: str, role: str, content: str) -> str:
    # Keep relay text natural so the receiving AI treats it as conversational context.
    # Dedup uses side-channel tracking, not an in-band machine marker.
    return f"[{source_label} says]: {content}"


def relay_to_peer(target_sock: str, source_label: str, role: str, event_id: str, content: str) -> str:
    # Use wibwob_ask so the receiving instance's AI participates in the same group chat.
    text = format_relay_prompt(source_label, role, content)
    return exec_command(target_sock, "wibwob_ask", text=text)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Inter-instance WibWob group chat broker")
    p.add_argument("--poll-interval", type=float, default=2.0, help="Seconds between polls (default: 2.0)")
    p.add_argument("--max-turns", type=int, default=20, help="Max successful broadcasts before stopping")
    p.add_argument("--cooldown", type=float, default=5.0, help="Min seconds between sends to same target")
    p.add_argument("--token-budget", type=int, default=4000, help="Approx token budget before stop")
    p.add_argument("--require", type=int, default=2, help="Minimum live instances required (default: 2)")
    p.add_argument(
        "--roles",
        default="user,assistant",
        help="Comma-separated roles to broadcast (default: user,assistant)",
    )
    return p.parse_args()


def main() -> int:
    args = parse_args()
    enabled_roles: Set[str] = {r.strip() for r in args.roles.split(",") if r.strip()}

    last_seen_counts: Dict[str, int] = {}
    last_relay_to_target_at: Dict[str, float] = {}
    delivered_to_target: Set[Tuple[str, str]] = set()  # (target_sock, exact injected text)
    delivered_events: Set[Tuple[str, str]] = set()     # (target_sock, event_id)
    seen_origin_events: Set[str] = set()               # event_id values already processed
    relayed_turns = 0
    token_spend_est = 0

    print(
        f"[broker] start poll={args.poll_interval}s max_turns={args.max_turns} "
        f"cooldown={args.cooldown}s token_budget={args.token_budget} roles={sorted(enabled_roles)}"
    )

    try:
        while True:
            socks = discover_sockets()
            if len(socks) < args.require:
                print(f"[broker] waiting for >= {args.require} sockets; found {len(socks)}")
                time.sleep(args.poll_interval)
                continue

            for sp in socks:
                last_seen_counts.setdefault(sp, 0)

            dead = [sp for sp in list(last_seen_counts.keys()) if sp not in socks]
            for sp in dead:
                print(f"[broker] socket disappeared: {sp}")
                last_seen_counts.pop(sp, None)
                last_relay_to_target_at.pop(sp, None)
                delivered_to_target = {pair for pair in delivered_to_target if pair[0] != sp}
                delivered_events = {pair for pair in delivered_events if pair[0] != sp}

            for source_sock in socks:
                source_label = pretty_instance_label(source_sock)
                try:
                    msgs = get_chat_history(source_sock)
                except Exception as e:
                    print(f"[broker] read fail {source_label}: {e}")
                    continue

                prev = last_seen_counts.get(source_sock, 0)
                if len(msgs) < prev:
                    print(f"[broker] history reset detected on {source_label} ({prev} -> {len(msgs)})")
                    prev = 0

                new_msgs = msgs[prev:]
                last_seen_counts[source_sock] = len(msgs)

                if not new_msgs:
                    continue

                for offset, msg in enumerate(new_msgs, start=prev):
                    role = str(msg.get("role", "")).strip().lower()
                    content = str(msg.get("content", ""))

                    if role not in enabled_roles:
                        continue
                    if not content.strip():
                        continue
                    if is_relay_message(source_sock, content, delivered_to_target):
                        # Broker-injected copies appear in target histories as new local user messages.
                        # Skip only the exact text the broker injected; genuine AI replies continue.
                        continue

                    event_id = make_event_id(source_label, role, offset, content)
                    if event_id in seen_origin_events:
                        continue
                    seen_origin_events.add(event_id)

                    est = estimate_tokens(content)
                    if token_spend_est + est > args.token_budget:
                        print(f"[broker] token budget reached: {token_spend_est}+{est} > {args.token_budget}")
                        return 0

                    for target_sock in socks:
                        if target_sock == source_sock:
                            continue

                        target_label = instance_name(target_sock)
                        key = (target_sock, event_id)
                        if key in delivered_events:
                            continue

                        now = time.time()
                        last_at = last_relay_to_target_at.get(target_sock, 0.0)
                        if args.cooldown > 0 and (now - last_at) < args.cooldown:
                            print(
                                f"[broker] cooldown skip {source_label}->{target_label} "
                                f"({now - last_at:.1f}s < {args.cooldown:.1f}s) id={event_id}"
                            )
                            continue

                        try:
                            resp = relay_to_peer(target_sock, source_label, role, event_id, content)
                        except Exception as e:
                            print(f"[broker] relay fail {source_label}->{target_label} id={event_id}: {e}")
                            continue

                        if resp.startswith("err"):
                            print(f"[broker] relay rejected {source_label}->{target_label} id={event_id}: {resp}")
                            continue

                        relayed_text = format_relay_prompt(source_label, role, content)
                        delivered_to_target.add((target_sock, relayed_text))
                        delivered_events.add(key)
                        last_relay_to_target_at[target_sock] = now
                        relayed_turns += 1
                        token_spend_est += est
                        print(
                            f"[broker] relay #{relayed_turns}: {source_label}/{role}->{target_label} "
                            f"id={event_id} chars={len(content)} est_tokens={est} "
                            f"budget={token_spend_est}/{args.token_budget} resp={resp}"
                        )

                        if relayed_turns >= args.max_turns:
                            print(f"[broker] max turns reached ({args.max_turns}); stopping")
                            return 0

            time.sleep(args.poll_interval)

    except KeyboardInterrupt:
        print("\n[broker] stopped by user")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

### Broker Notes (behavior and tradeoffs)

- The script uses `wibwob_ask` for fanout so each receiving AI gets the message as prompt input and can respond.
- Attribution is embedded as natural conversational text (for example, `[Instance 1 says]: ...`).
- Conversation loops are expected behavior in a multi-AI group chat: each AI sees relayed messages as user prompts and may respond.
- Dedup is intentionally narrower and two-layered:
  - skips exact broker-injected prompt text when it reappears in a target's history (copy suppression only)
  - tracks `(target_sock, event_id)` deliveries (send dedup)
- `chat_receive` is still useful for a future display-only mode where AIs should not respond.

## Why the Broker Uses Percent-Encoding (Not Base64 for `text=`)

Most likely footgun.

- `tools/api_server/ipc_client.py` base64-encodes only the `content` field (`tools/api_server/ipc_client.py:58-67`)
- Generic `exec_command` params are percent-decoded in `app/api_ipc.cpp:379-392`
- Base64 decode is only applied in `send_text content=...` at `app/api_ipc.cpp:505-517`

Therefore:

- `cmd:exec_command name=wibwob_ask text=hello%20world` works
- `cmd:exec_command name=wibwob_ask text=base64:aGVsbG8gd29ybGQ=` will pass literal `base64:...` unless new C++ decode logic is added (out of scope)

## Step-by-Step Implementation Plan (One Pass)

1. Patch `app/wibwob_view.h` to add history types, storage, and `getHistoryJson()`
2. Patch `app/wibwob_view.cpp` to:
   - add JSON escape helper
   - record history in `addMessage`
   - clear history in `clear`
   - mirror streaming lifecycle into `chatHistory_`
   - implement `getHistoryJson()`
3. Patch `app/test_pattern_app.cpp` to add `api_get_chat_history(TTestPatternApp&)`
4. Patch `app/command_registry.cpp` to add extern/capability/dispatch for `get_chat_history`
5. Add `tools/monitor/chat_coordinator.py` (script above)
6. Build + verify phase-by-phase

## Verification Commands

### Phase 2: `get_chat_history` works

1. Build:

```bash
cmake --build build --target test_pattern -j4
```

2. Launch one instance:

```bash
WIBWOB_INSTANCE=1 ./build/app/test_pattern 2>/tmp/wibwob_1.log
```

3. Open Wib&Wob chat window in the app UI (F12).

4. Inject a message (existing command):

```bash
python3 - <<'PY'
import socket
s=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/tmp/wibwob_1.sock")
s.sendall(b"cmd:exec_command name=wibwob_ask text=hello%20from%20ipc\n")
print(s.recv(4096).decode().strip())
s.close()
PY
```

5. Poll history:

```bash
python3 - <<'PY'
import socket, json
s=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/tmp/wibwob_1.sock")
s.sendall(b"cmd:exec_command name=get_chat_history\n")
raw=s.recv(65535).decode().strip()
print(raw)
obj=json.loads(raw)
print("messages:", len(obj["messages"]))
print("roles:", [m["role"] for m in obj["messages"]])
s.close()
PY
```

Expected:
- Valid JSON
- Includes at least one `"user"` message
- Includes `"assistant"` once the model responds (streaming or fallback)

### Phase 3: Group chat fanout between two instances

1. Terminal A:

```bash
WIBWOB_INSTANCE=1 ./build/app/test_pattern 2>/tmp/wibwob_1.log
```

2. Terminal B:

```bash
WIBWOB_INSTANCE=2 ./build/app/test_pattern 2>/tmp/wibwob_2.log
```

3. Open Wib&Wob chat window (F12) in both instances.

4. Terminal C (broker):

```bash
python3 tools/monitor/chat_coordinator.py --max-turns 10 --poll-interval 2 --cooldown 5
```

5. Trigger conversation by injecting into instance 1:

```bash
python3 - <<'PY'
import socket
s=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/tmp/wibwob_1.sock")
s.sendall(b"cmd:exec_command name=wibwob_ask text=Say%20hello%20to%20the%20other%20instance%20in%20one%20sentence.\n")
print(s.recv(4096).decode().strip())
s.close()
PY
```

Expected:
- Instance 1 records user + assistant messages
- Broker sees new messages via `get_chat_history`
- Broker broadcasts user and assistant messages to instance 2 with natural attribution text
- Instance 2 responds to relayed prompts
- Broker suppresses rebroadcast of broker-injected copies by exact-text side-channel dedup
- Conversation continues until `--max-turns` is hit

### Phase 4: Safety controls

#### Max turns

```bash
python3 tools/monitor/chat_coordinator.py --max-turns 3
```

Expected: exits after exactly 3 successful broadcasts.

#### Cooldown

```bash
python3 tools/monitor/chat_coordinator.py --cooldown 10 --poll-interval 1
```

Expected: log shows `cooldown skip ...` lines; fanout rate throttled.

#### Token budget guard

```bash
python3 tools/monitor/chat_coordinator.py --token-budget 100 --max-turns 99
```

Expected: exits early with `token budget reached ...`.

## Acceptance Criteria

- [x] AC-1: Message injection exists and triggers LLM response (`wibwob_ask`)
  - Test: `cmd:exec_command name=wibwob_ask text=hello` returns success and chat shows a new user message + assistant reply
- [x] AC-2: Display-only remote message receipt exists (`chat_receive`)
  - Test: `cmd:exec_command name=chat_receive sender=test text=hello` shows a message without triggering an LLM response
- [ ] AC-3: `get_chat_history` returns valid JSON with role+content messages
  - Test: `cmd:exec_command name=get_chat_history` returns `{"messages":[...]}` parseable JSON
- [ ] AC-4: Streaming assistant replies appear in `get_chat_history`
  - Test: Ask a prompt that streams; verify at least one `"assistant"` entry is present after completion
- [ ] AC-5: Broker broadcasts both user and assistant messages with attribution to other instances
  - Test: Start 2 instances + broker, inject one prompt, verify broker logs relays for `user` and `assistant` roles
- [ ] AC-6: Broker dedup / loop suppression works
  - Test: Verify exact broker-injected prompt copies are not rebroadcast, while genuine AI replies continue relaying
- [ ] AC-7: Human can inject into any instance mid-conversation
  - Test: While broker runs, type in instance 2; instance 1 receives relayed message and continues conversation
- [ ] AC-8: Safety controls stop or throttle fanout
  - Test: `--max-turns`, `--cooldown`, and `--token-budget` each produce the documented behavior

## Known Gotchas / Failure Modes

1. `wibwob_ask` requires the Wib&Wob chat window to be open
   - Error: `err no wibwob chat window open`
   - Broker logs relay rejects if target instance has no open chat window

2. Async injection is event-posted, not immediate
   - `api_wibwob_ask(...)` posts event `0xF0F0` in `app/test_pattern_app.cpp:3066-3071`
   - `TWibWobWindow::handleEvent(...)` drains `pendingAsk_` when engine is idle in `app/wibwob_view.cpp:513-523`
   - Aggressive polling may see user messages before assistant replies; expected

3. Streaming assistant replies bypass `addMessage()`
   - Must mirror history in streaming methods or `get_chat_history` misses assistant output

4. Base64 trap for `exec_command text=...`
   - Use percent encoding for `text=` in the broker

5. Chat clear/reset invalidates cursors
   - Broker handles `len(history) < last_seen` by resetting that socket cursor

6. Group chat can amplify cost quickly
   - Expected in multi-agent fanout
   - `--max-turns`, `--cooldown`, and `--token-budget` are mandatory guardrails

7. Role semantics on receiving instances are flattened when using `wibwob_ask`
   - Remote messages are injected as prompt text with natural attribution prefix
   - Future enhancement: hybrid `chat_receive` + model-context injection path if role preservation becomes necessary

## Minimal Patch Checklist (for the execution pass)

Before stopping, verify:

1. `get_chat_history` appears in `get_command_capabilities()` and dispatches in `exec_registry_command(...)`
2. `cmd:exec_command name=get_chat_history` returns valid JSON object with `messages` list
3. Streaming assistant replies are present in `messages`
4. `tools/monitor/chat_coordinator.py` runs with no third-party dependencies
5. Broker broadcasts user + assistant messages and suppresses rebroadcast of exact broker-injected copies
6. Safety guards work (`--max-turns`, `--cooldown`, `--token-budget`)

## Related

- `tools/monitor/instance_monitor.py` (socket discovery pattern)
- `tools/api_server/ipc_client.py` (IPC wire format reference)
