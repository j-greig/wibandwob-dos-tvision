---
id: E014
title: Inter-Instance Group Chat — get_chat_history + Broker
status: not-started
issue: 91
pr: ~
depends_on: []
branch: epic/e014-inter-instance-chat
---

# E014 — Inter-Instance Group Chat

## TL;DR

AC-1/AC-2 already shipped. Land `get_chat_history` (C++, ~50 lines) and a Python group-chat broker so multiple WibWob-DOS instances share one conversation.

## Source Spike

`.planning/spikes/.trash/spk-inter-instance-chat/spike-brief.md` — contains exact insertion points, full broker script, and verification commands. Read it before touching any code.

## What's Already Done (do not re-implement)

- `wibwob_ask` — injects user message, triggers LLM response (`command_registry.cpp:96`)
- `chat_receive` — display-only remote message receipt (`command_registry.cpp:95`)
- `TWibWobWindow::injectUserMessage()` — public async injection method (`wibwob_view.h:153`)
- Async drain loop via `pendingAsk_` in idle (`wibwob_view.cpp:516`)

## Features / Stories

### F01 — get_chat_history C++

- [ ] S01: Add `HistoryEntry` struct + `chatHistory_` buffer + `getHistoryJson()` to `TWibWobMessageView` (`wibwob_view.h`)
  Test: method compiles, returns `[]` on empty history
- [ ] S02: Hook history recording into `addMessage()` and `clear()` (`wibwob_view.cpp`)
  Test: user messages appear in `get_chat_history` output
- [ ] S03: Mirror streaming lifecycle into `chatHistory_` — `startStreamingMessage`, `appendToStreamingMessage`, `finishStreamingMessage`, `cancelStreamingMessage`
  Test: streamed assistant replies appear in history after completion; cancelled streams are removed
- [ ] S04: Add `api_get_chat_history()` IPC bridge near `api_wibwob_ask` (`test_pattern_app.cpp:3026`)
  Test: returns `{"messages":[...]}` JSON; returns `err no wibwob chat window open` when none open
- [ ] S05: Register `get_chat_history` in capabilities + dispatch (`command_registry.cpp`)
  Test: appears in `GET /api/capabilities`; `cmd:exec_command name=get_chat_history` dispatches correctly

### F02 — Python Group Chat Broker

- [ ] S06: Create `tools/monitor/chat_coordinator.py` (full script in spike brief)
  Test: runs with no third-party deps; broadcasts user + assistant messages between 2 instances with attribution; dedup suppresses broker-echo copies; `--max-turns`, `--cooldown`, `--token-budget` each stop/throttle as documented

## Acceptance Criteria

| AC | Criterion | Test |
|----|-----------|------|
| AC-3 | `get_chat_history` returns valid JSON | `cmd:exec_command name=get_chat_history` → parseable `{"messages":[...]}` |
| AC-4 | Streaming replies in history | Ask prompt that streams; `"assistant"` entry present after completion |
| AC-5 | Broker broadcasts with attribution | 2 instances + broker; inject prompt; broker logs relays for both roles |
| AC-6 | Broker dedup works | Broker-injected copies not rebroadcast; genuine AI replies continue |
| AC-7 | Human can inject mid-conversation | Type in instance 2 while broker runs; instance 1 receives relayed message |
| AC-8 | Safety controls work | `--max-turns 3` exits after 3 broadcasts; cooldown + token-budget behave as documented |

## Critical Gotchas (from spike)

1. **Streaming replies bypass `addMessage()`** — must hook all 4 streaming lifecycle methods or assistant output is missing from history
2. **Async injection** — `wibwob_ask` posts event `0xF0F0`, not immediate; broker may poll before reply arrives
3. **Percent-encode `text=`** in broker — NOT base64 (base64 only applies to `send_text content=`)
4. **`wibwob_ask` requires chat window open** — broker logs rejects if target has no open chat window
5. **History reset detection** — broker handles `len(history) < last_seen` by resetting that socket cursor

## Rollback

All C++ changes are additive (new struct fields + new methods). Removing `get_chat_history` dispatch branch restores prior state. Python broker is standalone with no app coupling.
