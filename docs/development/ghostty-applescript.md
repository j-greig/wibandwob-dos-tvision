# Launching TUI apps in Ghostty via AppleScript

Ghostty ships a full AppleScript dictionary (v1.3+, preview feature). Discover it anytime with:

```sh
sdef /Applications/Ghostty.app
```

Official docs: https://ghostty.org/docs/features/applescript

## Gotchas (learned the hard way, 2026-08)

- **PATH is bare.** Surface config `command` runs via `/usr/bin/login ... --noprofile --norc` — no user PATH, no `~/.bun/bin`, no brew. Always use absolute binary paths, or `zsh -c 'PATH=... cmd'` won't save you either if the login wrapper strips it. Test the exact command string in a plain shell first.
- **Terminology needs the tell block.** `input text` / `send key` only compile inside `tell application "Ghostty" ... end tell`. Outside it, AppleScript's built-in `text` class collides → cryptic syntax errors.
- **`new window`, never `make new window`.** The Standard Suite `make` returns an unusable reference (error -2710, by design).
- **Delay after window creation.** The surface needs ~0.5-1.5s to spawn its shell before `input text` lands.
- **`send key` is flaky for printables.** Use `input text` for text, `send key` only for `"enter"` and modifier chords.
- **`perform action` uses `on`, not `to`:** `perform action "decrease_font_size:2" on t`, `"toggle_fullscreen"`, etc. Smaller font = more cells = higher TUI resolution.
- **Pixel screenshots:** Ghostty windows via `screencapture -x -o -l<CGWindowID>`; find the ID with Quartz `CGWindowListCopyWindowInfo` (AX attributes don't expose it).

## Launch a binary in a new window

```sh
osascript <<'EOF'
tell application "Ghostty"
    set cfg to new surface configuration
    set command of cfg to "/absolute/path/to/binary"
    set wait after command of cfg to true
    set w to new window with configuration cfg
    activate window w
    return id of w
end tell
EOF
```

Example: `build/demos/demo_golem`, `demo_winwin`, `demo_corpus` (build first:
`cmake -B build/demos -S app/demos && cmake --build build/demos -j8`).

## Surface configuration properties

- `command` — run instead of shell
- `initial working directory`
- `initial input` — text sent after launch
- `wait after command` — keep window open on exit
- `font size`

## Driving a running surface

- `new tab in w with configuration cfg` — tabs in same window
- `input text "..." to terminal` — paste-style input
- `send key` / `send mouse button` / `send mouse position` / `send mouse scroll`
- `perform action "..."` — any Ghostty action string
- `close window w` / `activate window w`

Terminals are addressable: `focused terminal of selected tab of front window`.
