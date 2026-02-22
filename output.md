# tvterm Freeze Diagnosis Report

## Summary

The freeze is most likely caused by **synchronous PTY/process creation (`forkpty`) on the TUI main thread** when opening a terminal window.

This happens in the `cmOpenTerminal` command path before the next `idle()` tick, which matches your log evidence (`[idle] tick 5000` appears once, then stops completely).

## Root Cause (with exact lines)

### 1. Terminal creation runs on the UI event thread
- `app/test_pattern_app.cpp:1170-1178`
- `createTerminalWindow(r)` is called directly inside the `cmOpenTerminal` event handler.
- Because this is inside command handling, any blocking call here freezes the entire TUI event loop.

Relevant call site:
- `app/test_pattern_app.cpp:1173` calls `createTerminalWindow(r)`.

### 2. `createTerminalWindow(...)` synchronously creates a `TerminalController`
- `app/tvterm_view.cpp:161-169`
- `TerminalController::create(...)` is called before the window is returned/inserted.

Relevant line:
- `app/tvterm_view.cpp:166` calls `TerminalController::create(viewSize, factory, onTermError)`.

### 3. `TerminalController::create(...)` synchronously calls `createPty(...)`
- `vendor/tvterm/source/tvterm-core/termctrl.cc:82-90`
- The PTY is created **before** tvterm starts its reader/writer background threads.

Relevant lines:
- `vendor/tvterm/source/tvterm-core/termctrl.cc:86-89`

### 4. `createPty(...)` calls `forkpty(...)` directly (blocking point)
- `vendor/tvterm/source/tvterm-core/pty.cc:30-38`
- `forkpty(...)` is executed on the same main/UI thread that is handling `cmOpenTerminal`.

Blocking call:
- `vendor/tvterm/source/tvterm-core/pty.cc:37` (`forkpty(&masterFd, ...)`)

## Why it freezes only after Scramble async calls worked

This is consistent with calling `forkpty` in a process that is already multithreaded (your async LLM/Scramble activity proves threads are active).

`fork`/`forkpty` from a multithreaded process is a common deadlock/hang source because libc/runtime/internal locks may be held by other threads at fork time (or in atfork handlers). If the main thread blocks in `forkpty`, the TUI loop stops immediately, which matches the observed halt after the last idle tick.

This is an inference from the code path + timing evidence; the exact internal lock depends on runtime/library state.

## What is *not* the primary cause (for this symptom)

### Idle broadcast (`cmCheckTerminalUpdates`) is not doing PTY I/O on the main thread
- `app/test_pattern_app.cpp:2585-2586` broadcasts terminal updates each `idle()` tick.
- `vendor/tvterm/source/tvterm-core/termview.cc:74-78` only checks an atomic flag (`stateHasBeenUpdated()`) and calls `drawView()` if set.

This path can trigger drawing work, but it does **not** perform `read()`/`waitpid()`/PTY polling directly on the main thread.

### PTY `read()` happens in tvterm reader thread, not UI thread
- `vendor/tvterm/source/tvterm-core/termctrl.cc:181-188` (`runReaderLoop`)
- `vendor/tvterm/source/tvterm-core/pty.cc:77-99` (`PtyMaster::readFromClient` with `read()`)

### `waitpid()` appears in disconnect/shutdown path, not open/update path
- `vendor/tvterm/source/tvterm-core/pty.cc:128-138`
- This is `PtyMaster::disconnect()` cleanup logic, not terminal creation.

### Input Grab mode is not automatically entered on open
- Input grab is triggered by explicit `cmGrabInput` handling in `BasicTerminalWindow::handleEvent`
- `vendor/tvterm/source/tvterm-core/termwnd.cc:100-105`
- It enters modal execution via `owner->execView(this)` (`termwnd.cc:103`) only when that command is sent.

So Input Grab is not the reason for the immediate freeze when merely opening the terminal window.

## Minimal Fix Proposal

### Preferred minimal fix (robust): replace `forkpty` path with `openpty + posix_spawn`

Change tvterm PTY creation on Unix/macOS to avoid `forkpty` in the multithreaded app process:

- In `vendor/tvterm/source/tvterm-core/pty.cc`
- Replace `forkpty(...)` (`pty.cc:37`) with:
  - `openpty(...)` to create master/slave fds
  - `posix_spawn(...)` (or `posix_spawnp(...)`) with file actions to attach stdio to the slave fd
  - `setsid`/controlling TTY setup as needed via spawn attrs / child setup strategy

Why this is the right fix:
- `posix_spawn` is generally safe in multithreaded processes where `fork`/`forkpty` is risky.
- It preserves terminal behavior while removing the main-thread fork hazard.

## Quick Workarounds (pragmatic)

1. Open terminal windows before any async LLM/Scramble work starts.
- Avoids `forkpty` after the process becomes thread-busy.
- Crude but likely effective for immediate testing.

2. Disable `cmOpenTerminal` after async engine initialization (temporary guard).
- Prevents hard freezes in user sessions until PTY spawn is fixed.

3. Pre-spawn one terminal at app startup and reuse/show it later.
- Still uses `forkpty`, but if done early (before async activity/threads), it may avoid the hang.
- This is a workaround, not a real fix.

## Additional Note (secondary risk, not current root cause)

There is a reentrancy risk in tvterm drawing:
- `vendor/tvterm/source/tvterm-core/termview.cc:215-222`
- `TerminalView::draw()` holds `termCtrl.lockState(...)` and sends a synchronous message to the owner (`cmTerminalUpdated`) while the terminal state lock is held.

That is not the freeze signature you reported, but it is a fragile pattern and could cause deadlocks if owner-side handlers ever re-enter terminal state.

## Recommended next step

Instrument the spawn path to confirm the exact stuck line in your build:
- log before/after `createTerminalWindow(...)` call (`app/test_pattern_app.cpp:1173`)
- log before/after `TerminalController::create(...)` (`app/tvterm_view.cpp:166`)
- log before/after `forkpty(...)` (`vendor/tvterm/source/tvterm-core/pty.cc:37`)

You should see logs stop at or immediately before the `forkpty` call in the freeze case.
