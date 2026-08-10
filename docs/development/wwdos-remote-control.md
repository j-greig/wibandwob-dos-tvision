# Remote-controlling WibWob-DOS (TS app) — no key puppeting needed

The bun TUI app (`~/Repos/wibandwob-dos`) exposes a **control API on
http://127.0.0.1:8099** — use it instead of AppleScript keystrokes for
everything except launching.

## Launch (in Ghostty, via AppleScript)

Absolute bun path is mandatory (login shell has no user PATH):

```applescript
tell application "Ghostty"
    set cfg to new surface configuration
    set command of cfg to "/bin/zsh -c 'cd /Users/james/Repos/wibandwob-dos && WIBWOB_INSTANCE_LABEL=dev PATH=$PATH:/Users/james/.bun/bin /Users/james/.bun/bin/bun run src/app.ts --dev'"
    set wait after command of cfg to true
    new window with configuration cfg
end tell
```

## Control API cheatsheet

- `GET /openapi.json` — full route list (~40 endpoints)
- `GET /commands/list` — every command id (menus/palette equivalents)
- `POST /commands/run` `{"id":"microapp.wibwob.world.open"}` — open any app
- `POST /commands/run` `{"id":"microapp.wibwob.world.set-render-mode","args":{"mode":"flight"}}`
- `POST /windows/input` `{"id":"1","input":"up"}` — **`id` is a STRING**; a
  `windowId` number here fails with "Window NaN not found" (though
  `/windows/maximize` accepts `{"windowId":1}` — inconsistent, check schema)
- `GET /screenshot/text` / `/screenshot/ansi` — grid dumps (the `/screenshot`
  route returns text, not pixels)
- `/windows/move` needs `left`/`top`, not `x`/`y`

## Gotchas

- **Desktop doesn't reflow on terminal resize.** After resizing the Ghostty
  window/font, inner windows stay at the old grid size and `/windows/resize`
  silently clamps. Fix: run `window.toggle_maximize` **twice** via
  `/commands/run` to force a refit.
- MCP server also runs (port 8109, `/mcp`, 73 endpoints) if you'd rather
  drive it as MCP tools.
- Pixel screenshots: `screencapture -x -o -l<CGWindowID>` (Quartz
  `CGWindowListCopyWindowInfo` to find the window number by title).

## WibWobWorld flight mode keys (via /windows/input)

`m` cycle render mode · `+`/`-` throttle · arrows/wasd fly · `,`/`.` pitch ·
`[`/`]` altitude · space stop · `r` reseed · `t` cycle terrain · `q` close
