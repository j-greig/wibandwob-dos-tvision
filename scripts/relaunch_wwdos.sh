#!/bin/bash
# relaunch_wwdos.sh — kill any running wwdos, CLOSE its dead Ghostty windows,
# spawn a fresh one. NO STRAYS:
#   1. wait-after-command surfaces show "Press any key to close" after the
#      process dies — we send that key. Dead wwdos windows are identified by
#      their 👻 title so user shells are never touched.
#   2. New surfaces still request wait-after-command=false; the keypress
#      sweep is the belt to that braces (Ghostty has ignored the property).
# History: window ids are STRINGS (tab-group-...) — closing "by id" with an
# unquoted id fails silently, which is how strays piled up. Never again.
set -u

# Auto-save the live layout before killing — a relaunch must NEVER eat a
# human's composition (learned 2026-08-13, sorry Zilla). Best-effort: if the
# API is down there is nothing to save anyway.
curl -s -m 3 -X POST http://localhost:8089/menu/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"save_workspace","args":{"path":"workspaces/pre_relaunch.json"}}' >/dev/null 2>&1

pkill -f "build/app/wwdos" 2>/dev/null
sleep 1

# Sweep: press a key into every dead 👻 window so it closes itself
osascript <<'EOF' 2>/dev/null
tell application "System Events"
  if not (exists process "Ghostty") then return
  tell process "Ghostty"
    set frontmost to true
    repeat with w in windows
      try
        set t to title of w
        if t is "👻" or t is "" then
          perform action "AXRaise" of w
          delay 0.2
          keystroke " "
          delay 0.2
        end if
      end try
    end repeat
  end tell
end tell
EOF

sleep 1
osascript <<'EOF'
tell application "Ghostty"
    set cfg to new surface configuration
    set command of cfg to "/Users/james/Repos/wibandwob-dos-tvision/scripts/launch_wwdos_ghostty.sh"
    set wait after command of cfg to false
    set w to new window with configuration cfg
    activate window w
end tell
EOF
# Size the window LARGE (Zilla 2026-08-13: test at ~2x for realistic
# window real estate; screenshots at small sizes lie about layouts)
sleep 1
osascript <<'EOF2' 2>/dev/null
tell application "System Events" to tell process "Ghostty"
    set position of front window to {60, 40}
    set size of front window to {2400, 1360}
end tell
EOF2

echo "wwdos relaunched (stray sweep done, sized 2400x1360)"
