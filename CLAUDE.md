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
- `TColor` has no `uchar` constructor (magiblot/tvision since d726902): a BIOS colour number held in a `uchar` becomes an RGB value, so `TColorAttr(fg, bg)` with `uchar`s draws near-black. Pass `TColorBIOS(x)`. To find such calls, add `TColor(unsigned char) = delete;` to `vendor/tvision/include/tvision/colors.h` and build.
- `open_terminal exec=<cmd>` runs `<cmd>` through `tools/term_exec.sh`, found from the app's cwd (run the app from the repo root) or `WWDOS_REPO`. The app can inherit SIGHUP ignored and tvterm does not reliably hang up a closed window's child, so the wrapper resets the signals and writes a pidfile that the window kills on close. A plain shell terminal still leaves its shell running after close.
- A nested WIBWOB-DOS writes stderr into its own terminal unless redirected; `/show/nest` sends it to `/tmp/wwdos-nest<k>.log`.
- `tmux capture-pane` drops trailing blank cells unless given `-N`, so coloured spaces at a line's end read back as default black.
- IPC latency: the app serves its socket from `idle()`. A helper thread in `api_ipc.cpp` calls `TEventQueue::wakeUp()` when a client connects; without it each round trip waits for the 20 ms event timeout. The server reads one newline-terminated command of any length per connection (the old single 2048-byte read cut long commands), and `cmd:batch` runs many commands in one message.
- TR-808 drum machine (`app/tr808_*`, window type `tr808`): `POST /tr808 {action}` drives it; step hits stream on `GET /events?types=tr808_step` through the relay in `tools/api_server/tr808.py` (one IPC `subscribe_events` connection). Launch with `WWDOS_TR808_AUDIO=0` when a web page plays the sound. Docs: `tools/api_server/README.md` § TR-808 drum machine.
- A fresh git worktree has empty `vendor/` submodules (tvterm has nested ones): `git submodule update --init --reference <main checkout>/vendor/<name> vendor/<name>`, and the same inside `vendor/tvterm` for `deps/tvision` and `deps/vterm/libvterm`, before `cmake -B`.
- Terminal films: the stage driver is `app/stage/` (grammar comment in `stage.h`, command table `commands.def`), the
  film desktop `app/film/`; target `film` in `kernel/build-web.sh`, native `cmake -B build-stage -S app/film`. The desktop
  app (`wwdos`) does not link it and cannot play a stage. Detail: `docs/stage-driver.md`.
- The on-chain repo (`wibandwob-dos-onchain`) builds films from a pinned copy (`tools/vendor_sync.py` there; `--check`,
  `--to-core`). Keep `app/film/CMakeLists.txt` flags and source order equal to its `wwdos-min` target: the two wasm
  files are compared byte for byte. Its token build is frozen; `app/film/boot/` is compared there, never copied.
- `tfilm/base/film.html` is the built film page, byte-identical to the on-chain `dist/wwdos-min-webgl.html`. After a
  change to the film engine, its views, `kernel/pages/` or `kernel/tools/bundle.py`, run `tfilm/scripts/check-base.sh`
  and commit the new page with the change when the difference is intended.
- After changing `app/stage/stage.cpp`'s lists or adding a command: `python3 tests/stage/test_commands_def.py`.
- Director surface: `tools/api_server/show_ops.py` (batch, names, place, scene, timeline, dive). Window names live in the API process, not the app: restarting the API forgets them, and windows then show their title or id in `/show/scene`.
- Resident input (`app/resident_input.cpp`, `docs/resident-input.md`): `resident_key`, `resident_mouse`, `resident_press`, `resident_menu` with `id=` reach a window without changing `deskTop->current`; they join the person's events in `TWwdosApp::getEvent`/`putEvent`, so a new override of either must keep calling `resident::takeEvent`, `afterBase` and `interceptPut`.
- Resident commands queue and reply with seq numbers before anything is delivered; wait for the seqs in `resident_status since=` before reading the screen or `window_text`.
- `putEvent` (and `inject_command`) holds one event: a second in the same tick replaces the first. Queue several through the resident commands instead.
- Typing as the person through tmux needs one key per write: three or more printable keys in one read arrive as a paste (`kbPaste`), which `TTextEditorView` drops.
- `tests/resident/test_resident_input.py` starts its own desktop (`tmux -L residenttest`, its own socket); `WWDOS_BIN` picks the binary. Opening a `backrooms_tv` window stops the app (unmodified build too).
