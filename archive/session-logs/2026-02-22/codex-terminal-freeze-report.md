Findings are written to `output.md`.

Bottom line: the freeze is most likely caused by `tvterm` calling `forkpty()` synchronously on the TUI main thread during terminal window creation (`cmOpenTerminal` path), not by the idle broadcast/update handler.

Key references:
- Main-thread open path: `app/test_pattern_app.cpp:1170`, `app/test_pattern_app.cpp:1173`
- Wrapper create call: `app/tvterm_view.cpp:161`, `app/tvterm_view.cpp:166`
- tvterm create path: `vendor/tvterm/source/tvterm-core/termctrl.cc:82`, `vendor/tvterm/source/tvterm-core/termctrl.cc:87`
- Blocking call: `vendor/tvterm/source/tvterm-core/pty.cc:37` (`forkpty(...)`)

What I ruled out:
- Idle broadcast path (`cmCheckTerminalUpdates`) is not doing PTY I/O on main thread: `app/test_pattern_app.cpp:2585`, `vendor/tvterm/source/tvterm-core/termview.cc:74`
- PTY `read()` is in reader thread: `vendor/tvterm/source/tvterm-core/termctrl.cc:181`, `vendor/tvterm/source/tvterm-core/pty.cc:82`
- `waitpid()` is in disconnect/shutdown path: `vendor/tvterm/source/tvterm-core/pty.cc:133`
- Input Grab is explicit modal command, not automatic on open: `vendor/tvterm/source/tvterm-core/termwnd.cc:100`, `vendor/tvterm/source/tvterm-core/termwnd.cc:103`

Minimal fix proposal:
- Replace Unix/macOS `forkpty` spawn path with `openpty + posix_spawn` in `vendor/tvterm/source/tvterm-core/pty.cc` to avoid `fork` in a multithreaded process.

Quick workaround:
1. Open terminal before running Scramble/async LLM work.
2. Temporarily disable `cmOpenTerminal` after async engine starts.
3. Pre-spawn a terminal at startup and reuse it later.

If you want, I can implement a small instrumentation patch next (before/after logs around `createTerminalWindow`, `TerminalController::create`, and `forkpty`) to confirm the exact stuck line in your local build.