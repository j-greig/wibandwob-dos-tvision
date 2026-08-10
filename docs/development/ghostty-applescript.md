# Launching TUI apps in Ghostty via AppleScript

Ghostty ships a full AppleScript dictionary. Discover it anytime with:

```sh
sdef /Applications/Ghostty.app
```

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
