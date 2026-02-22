# Scramble Cat Async Reply Freeze Fix Report

## Summary

The freeze after async LLM reply delivery was caused by **unsanitized CLI/LLM text being rendered directly into Turbo Vision draw buffers** in Scramble views. Posting the reply through `cmScrambleReply` fixed the thread/event-loop delivery path, but it did not address control/ANSI bytes in the reply payload.

## Root Cause

### Where the freeze is triggered

- `app/test_pattern_app.cpp:1607` `TTestPatternApp::deliverScrambleReply()` moves the async reply into UI rendering.
- `app/test_pattern_app.cpp:1615` calls `scrambleWindow->getView()->say(reply)`.
- `app/test_pattern_app.cpp:1618` calls `scrambleWindow->getMessageView()->addMessage("scramble", reply)`.

### Why the event-refactor did not fix it

The async callback path already correctly queues the response and posts `cmScrambleReply` (main-thread delivery). However, the payload still reaches Scramble UI rendering without text sanitization.

- `app/scramble_engine.cpp:539` / `app/scramble_engine.cpp:541` apply `voiceFilter()` and invoke the callback.
- `app/scramble_engine.cpp:569` `voiceFilter()` lowercases and appends kaomoji, but **does not remove terminal control/ANSI escape sequences**.

### Exact unsafe render points (pre-fix)

- `app/scramble_view.cpp:190` `TScrambleView::say(...)`
- `app/scramble_view.cpp:192` stored reply text directly in `bubbleText`
- `app/scramble_view.cpp:461` `TScrambleMessageView::addMessage(...)`
- `app/scramble_view.cpp:465` stored reply text directly in message history

If the CLI output contains hidden control bytes (e.g. ESC/ANSI/OSC, CR controls), Turbo Vision rendering can corrupt terminal state and the TUI appears hard-frozen immediately after the reply is shown.

## Fix Implemented

### Changes

- Added `sanitizeScrambleDisplayText(...)` in `app/scramble_view.cpp:20`.
- Sanitizer removes:
  - ANSI CSI sequences (`ESC [ ...`)
  - OSC sequences (`ESC ] ... BEL` / `ESC \\`)
  - other ESC-prefixed sequences
  - ASCII control chars (except normalized newline/tab handling)
  - `DEL` (`0x7F`)
- Sanitizer normalizes `\r` to `\n` and tabs to spaces.

### Applied at render boundaries

- `app/scramble_view.cpp:192` in `TScrambleView::say(...)`
- `app/scramble_view.cpp:465` in `TScrambleMessageView::addMessage(...)`

This keeps the fix local to Scramble UI and does not affect Wib&Wob Chat behavior.

## Build / Test Verification

### Build

- Command: `cd build && make -j8 test_pattern`
- Result: Success

### Tests

- Command: `cd build && ctest -R scramble --output-on-failure`
- Result: Passed (`scramble_engine`)

## Notes

- I did not remove or alter Scramble `idle()` polling or the Scramble timer.
- The fix is compatible with the existing `cmScrambleReply` event delivery approach.
