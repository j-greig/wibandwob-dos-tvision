Only record gotchas and information that would not be obvious from a quick exploration of the filesystem.

- Don't tell the user what you are going to do next; act. Report findings and results.
- Build the app with `cmake --build build --target wwdos -j 8`. The default build includes tests; `command_registry_test` and `scramble_engine_test` currently fail to link against missing application symbols. A leftover CMake cache does not imply generated build files exist: rerun README configuration if Makefile is missing.
- The desktop runs without the Python API. Use a real terminal or persistent tmux session, and redirect stderr to a log so diagnostics do not corrupt the display. Check existing sessions before launching another desktop.
- Launch scripts have side effects: `scripts/dev-start.sh` restarts instance resources and removes its socket; `start_api_server.sh` kills listeners on its configured port. To add API control to an existing desktop, run from repo root in a separate persistent session:
  ```sh
  WIBWOB_INSTANCE=1 WIBWOB_REPO_ROOT="$PWD" tools/api_server/venv/bin/uvicorn tools.api_server.main:app --host 127.0.0.1 --port 8089
  ```
  Match `WIBWOB_INSTANCE` to the desktop (`/tmp/wibwob_1.sock` for instance 1). Install the API requirements into its venv first.
- Keep the MCP dependency constraint: `fastapi-mcp==0.4.0` requires MCP SDK 1.x. SDK 2.x crashes startup with `Server.__init__() takes 2 positional arguments but 3 were given`.
- REST and streamable HTTP MCP (`/mcp`) control the live desktop. `/health` proves only API liveness; verify `/state` and a visible operation. Read the live canvas dimensions before arranging windows.
- `/windows/{id}/move` was observed ignoring `w`/`h` for a text editor. Use `/menu/command` with `{"command":"resize_window","args":{"id":"…","w":"…","h":"…"}}` and verify state. Some apps ignore titles supplied during creation; the registry exposes `window_title`.
