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
- `build/` has a stale SDK path for `libncurses.tbd` and fails to link; `build-glitch/` is a fresh configure of the same source. `build/app/wwdos` is a copy of `build-glitch/app/wwdos` (older ones kept as `wwdos.pre-show-20261102`, `wwdos.pre-ega-20261104`, `wwdos.pre-speed-20261104`).
- Stop a test instance by its tmux session (`tmux kill-session -t <name>`) or its exact pid, never `pkill -f <binary path>`: the tmux server keeps the command line of the session that started it, so the pattern matches the server and kills every session on it, other agents' included.
- Themes: `app/ega_theme.cpp` owns the app palette (mono or EGA, both extended with eight window-colour families at indices 136-199) and per-window colours. A view that should follow the theme calls `ww::contentAttr()` in `draw()`; a window class gets coloured frames with `WW_COLOUR_PALETTE_OVERRIDE`. After a palette change call `ww::redrawDeep()`: windows keep a cached buffer, so `drawView()` repaints the old colours, and the application's own `draw()` only flushes the screen.
- `open_terminal exec=<cmd>` runs `<cmd>` through `tools/term_exec.sh`, found from the app's cwd (run the app from the repo root) or `WWDOS_REPO`. The app can inherit SIGHUP ignored and tvterm does not reliably hang up a closed window's child, so the wrapper resets the signals and writes a pidfile that the window kills on close. A plain shell terminal still leaves its shell running after close.
- A nested WIBWOB-DOS writes stderr into its own terminal unless redirected; `/show/nest` sends it to `/tmp/wwdos-nest<k>.log`.
- `tmux capture-pane` drops trailing blank cells unless given `-N`, so coloured spaces at a line's end read back as default black.
- IPC latency: the app serves its socket from `idle()`. A helper thread in `api_ipc.cpp` calls `TEventQueue::wakeUp()` when a client connects; without it each round trip waits for the 20 ms event timeout. The server reads one newline-terminated command of any length per connection (the old single 2048-byte read cut long commands), and `cmd:batch` runs many commands in one message.
- Director surface: `tools/api_server/show_ops.py` (batch, names, place, scene, timeline, dive). Window names live in the API process, not the app: restarting the API forgets them, and windows then show their title or id in `/show/scene`.
