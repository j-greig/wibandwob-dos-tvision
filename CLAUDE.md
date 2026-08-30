# wibandwob-dos-tvision

**Repo disambiguation** — two similarly-named repos, easy to conflate:
- `wibandwob-dos-tvision` (THIS repo): C++ / Turbo Vision build + vintage demos
- `~/Repos/wibandwob-dos`: TS / bun TUI — WibWob-DOS app, microapps (wibwobworld flightsim), control API :8099

When a session touches both, SAY SO explicitly at each switch: name the repo and branch before running git commands or editing files there.

- Build vintage demos standalone: `cmake -B build/demos -S app/demos && cmake --build build/demos -j8` (targets: `demo_golem`, `demo_winwin`, `demo_corpus`, +20 more)
- To launch/drive any TUI binary in Ghostty via AppleScript, see `docs/development/ghostty-applescript.md`
