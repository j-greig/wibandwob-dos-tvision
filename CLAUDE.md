# wibandwob-dos-tvision

**Repo disambiguation** — two similarly-named repos, easy to conflate:
- `wibandwob-dos-tvision` (THIS repo): C++ / Turbo Vision build + vintage demos
- `~/Repos/wibandwob-dos`: TS / bun TUI — WibWob-DOS app, microapps (wibwobworld flightsim), control API :8099

When a session touches both, SAY SO explicitly at each switch: name the repo and branch before running git commands or editing files there.

- Build vintage demos standalone: `cmake -B build/demos -S app/demos && cmake --build build/demos -j8` (targets: `demo_golem`, `demo_winwin`, `demo_corpus`, +20 more)
- To launch/drive any TUI binary in Ghostty via AppleScript, see `docs/development/ghostty-applescript.md`
- Show layer for live performances (typed reveal, glide, cursors, shape, fall, droste, chrome, capture, `/events` SSE): `tools/api_server/show.py`, documented in `tools/api_server/README.md` § Show endpoints. It calls IPC directly (no spawn-log files) and retries a dropped socket; plain `send_text` writes a `*-spawned.txt` file at repo root per call.
- The editor drops a `send_text` whose content is a lone newline; send it with the next character.
- `get_canvas_size` height includes the status line: desktop rows = height - 1, and desktop `y` 0 is the row under the menu bar.
- Build a second binary in `build-glitch/` (`cmake -B build-glitch -S . && cmake --build build-glitch --target wwdos`) when another agent runs `build/app/wwdos`; rebuilding `build/` under a running instance is avoidable risk.
- Replace a macOS binary with `rm` then `cp` (new inode). Copying over a binary that has run gets the new process killed at launch (stale code-signature cache) with nothing in the log.
- `build/` has a stale SDK path for `libncurses.tbd` and fails to link; `build-glitch/` is a fresh configure of the same source. `build/app/wwdos` is a copy of `build-glitch/app/wwdos` (old one kept as `wwdos.pre-show-20261102`).
