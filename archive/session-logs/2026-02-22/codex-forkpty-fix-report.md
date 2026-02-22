Implemented the Unix PTY fix in `vendor/tvterm/source/tvterm-core/pty.cc` by removing `forkpty()` and replacing it with `openpty()` + a manual child launch path that avoids the multithreaded `forkpty()` deadlock.

What changed:
- `createPty` now uses `openpty(&masterFd, &slaveFd, ...)` instead of `forkpty(...)` and keeps `onError(...)` handling (`vendor/tvterm/source/tvterm-core/pty.cc:40`).
- Added a Unix child launcher `spawnPtyClient(...)` that uses `vfork()` + `execve()` (`vendor/tvterm/source/tvterm-core/pty.cc:132`).
- Child setup now explicitly does:
  - `setsid()` (`vendor/tvterm/source/tvterm-core/pty.cc:154`)
  - `ioctl(slaveFd, TIOCSCTTY, 0)` for controlling terminal (`vendor/tvterm/source/tvterm-core/pty.cc:157`)
  - `dup2` of slave PTY to stdin/stdout/stderr (`vendor/tvterm/source/tvterm-core/pty.cc:167`)
  - default signal handlers restored (`vendor/tvterm/source/tvterm-core/pty.cc:162`)
- Preserved custom env behavior by building a merged environment (base env + overrides) before spawn and passing it to `execve()` (`vendor/tvterm/source/tvterm-core/pty.cc:73`, `vendor/tvterm/source/tvterm-core/pty.cc:135`).
- Kept exec-failure diagnostics to `stderr` in the child path (`vendor/tvterm/source/tvterm-core/pty.cc:178`).

Build verification:
- Ran `cd build && make -j8 test_pattern`
- Result: success (`test_pattern` built)
- Note: macOS emits a deprecation warning for `vfork()` (build still succeeds)

Why `vfork()` instead of `posix_spawn()`:
- `posix_spawn()` is not portable enough here for reliable `setsid()` + controlling terminal (`TIOCSCTTY`) setup on macOS/Linux in one path.
- This implementation preserves the terminal/session behavior while avoiding the `forkpty()` deadlock trigger in a multithreaded process.