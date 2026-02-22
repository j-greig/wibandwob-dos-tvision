# Scramble Async Freeze Diagnosis (2026-02-22)

## Summary

The freeze is most likely caused by **running Scramble UI redraws directly from the async completion callback while `TTestPatternApp::idle()` is executing**.

This is an async-refactor regression because the first LLM response now completes via `scrambleEngine.poll()` inside `idle()`, and the callback immediately calls methods that synchronously `drawView()`.

## 1. Root Cause (with line refs)

### Root cause: Scramble async completion callback performs immediate view redraws from `idle()`

- `TTestPatternApp::idle()` calls `scrambleEngine.poll()` on the main thread: `app/test_pattern_app.cpp:2539`, `app/test_pattern_app.cpp:2545`
- `ScrambleHaikuClient::poll()` invokes the stored callback inline (same call stack): `app/scramble_engine.cpp:347`, `app/scramble_engine.cpp:380`, `app/scramble_engine.cpp:384`
- The callback in `wireScrambleInput()` directly updates Scramble views: `app/test_pattern_app.cpp:1674`, `app/test_pattern_app.cpp:1676`, `app/test_pattern_app.cpp:1681`, `app/test_pattern_app.cpp:1684`
- Those updates synchronously redraw:
  - `TScrambleView::say()` calls `drawView()`: `app/scramble_view.cpp:121`, `app/scramble_view.cpp:126`
  - `TScrambleMessageView::addMessage()` calls `drawView()`: `app/scramble_view.cpp:390`, `app/scramble_view.cpp:397`

### Why this matches the symptom timing

- The freeze happens **right after the first async response appears** because that is the first time the async callback runs from `idle()`.
- The same file already documents that idle-time UI updates can cause freezes: `app/test_pattern_app.cpp:2547` ("causing crashes + freezes").

### What this is not (most likely)

- Not just `isBusy()` staying true: that would block Scramble interactions, but it would not explain the **whole TUI** becoming unresponsive.
- Not just a heavy callback body: the callback is small; the risky part is **where** it runs (`idle()`), not how much work it does.

## 2. Minimal Fix

### Fix approach

Do **not** touch/draw Scramble views directly inside the async callback fired from `poll()`.

Instead:

1. Store the response in a small pending queue/state.
2. Post a Turbo Vision command/broadcast event (e.g. `cmScrambleAsyncDone`).
3. Handle that event in normal event dispatch (`handleEvent`) and perform `say()` / `addMessage()` there.

### Minimal code change shape (conceptual)

- In the callback (currently at `app/test_pattern_app.cpp:1675`), replace direct UI calls with:
  - `pendingScrambleReply = r;`
  - `message(this, evBroadcast, cmScrambleAsyncDone, nullptr);` (or equivalent event-posting API used in this codebase)
- In `TTestPatternApp::handleEvent(...)`, handle `cmScrambleAsyncDone` and then call:
  - `scrambleWindow->getView()->say(...)`
  - `scrambleWindow->getMessageView()->addMessage(...)`

This keeps redraws out of the `idle()` callback call stack.

## 3. Other Async Issues Spotted

### A. `pclose()` can still block the UI thread in `poll()` and `cancelAsync()`

- `poll()` calls `pclose(activePipe)` in the UI-thread idle path: `app/scramble_engine.cpp:359`, `app/scramble_engine.cpp:360`
- `cancelAsync()` also calls blocking `pclose()` (close-window / shutdown path): `app/scramble_engine.cpp:386`, `app/scramble_engine.cpp:389`

Risk:
- `pclose()` waits for child termination. If the child exits slowly/hangs, the TUI can freeze.

Recommended follow-up:
- Replace `popen` async path with `fork/exec + pipe + waitpid(..., WNOHANG)` (store `pid_t`), or run subprocess work on a background thread and marshal results back to UI.

### B. `markCalled()` happens before async start success is known

- `haikuClient.markCalled()` is called before `startAsync(...)` returns: `app/scramble_engine.cpp:531`, `app/scramble_engine.cpp:532`, `app/scramble_engine.cpp:545`

Risk:
- Failed `popen`/startup still consumes the rate-limit window.

Minimal fix:
- Move `markCalled()` after successful `startAsync(...) == true`.

### C. Non-blocking `FILE*` + `fread()` is brittle and error handling is incomplete

- `fcntl(... O_NONBLOCK)` is applied to the `popen` fd: `app/scramble_engine.cpp:331`, `app/scramble_engine.cpp:338`, `app/scramble_engine.cpp:340`
- Polling uses `clearerr()` + `fread()` and checks only `feof()`: `app/scramble_engine.cpp:352`, `app/scramble_engine.cpp:353`, `app/scramble_engine.cpp:359`

Risk:
- `EAGAIN`/stream error states are masked and not distinguished from real errors.
- `stdio` buffering on non-blocking fds is fragile.

Recommended follow-up:
- Use `read()` on the fd and explicit `errno` handling instead of `fread()` on `FILE*`.

### D. Fixed temp file path for curl requests is race-prone

- Hard-coded `/tmp/scramble_haiku.json` in both sync and async curl builders: `app/scramble_engine.cpp:174`, `app/scramble_engine.cpp:274`

Risk:
- Overwrite/collision across concurrent calls/processes.

Minimal fix:
- Use `mkstemp()`/unique tempfile per request.

## Confidence / Notes

- This is a code-path diagnosis (no runtime debugger attached in this pass).
- The idle-callback redraw path is the strongest match for the exact symptom timing and existing in-file freeze warning.
