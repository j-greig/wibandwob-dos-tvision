# wibandwob-dos-tvision

**Repo disambiguation** — two similarly-named repos, easy to conflate:
- `wibandwob-dos-tvision` (THIS repo): C++ / Turbo Vision — the wwdos TUI app, control API :8089, vintage demos
- `~/Repos/wibandwob-dos`: TS / bun TUI — microapps (wibwobworld flightsim), control API :8099

When a session touches both, SAY SO explicitly at each switch: name the repo and branch before running git commands or editing files there.

## Quick reference

- Build app: `cmake --build build --target wwdos -j8` · demos: `cmake -B build/demos -S app/demos && cmake --build build/demos -j8`
- Run: `./scripts/launch_wwdos_ghostty.sh` (never bare — fixes PATH, enforces claude-login auth) then `./start_api_server.sh`
- **Operate/debug the app → `docs/development/wwdos-runbook.md`** — the ONE operational doc (launch, control API, Ghostty AppleScript, chat/LLM plumbing, gotchas). Read before driving wwdos; refine it when you learn something it lacks. No new md files: prune and fold into existing docs.
- Chat model is normalised in 3 places (config json / provider cpp / bridge js) — runbook § LLM plumbing.

## Standing directive: proactive symbient

Leave every session with the repo better than you found it. Concretely:

- Hit a missing control surface, dead endpoint, or undocumented behaviour? **Fix or build it in the same session** — don't work around it silently.
- New windows must spread, not stack: placement goes through `findSpreadRect()`; arrange art with `/gallery/arrange`, not uniform tiles.
- Learned a gotcha? It goes in the runbook the same day. Docs that lag the code are bugs.
- Prefer DRY + modular: one source of truth per behaviour (the 3-place model clamp is the cautionary tale — collapse duplication when you touch it).
- Verify visually: screenshot after driving the TUI; `ok:true` ≠ pixels changed.
