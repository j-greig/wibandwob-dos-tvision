#!/bin/bash
# relaunch_wwdos.sh — kill any running wwdos, close its old Ghostty window,
# spawn a fresh one, remember the window id. Prevents stray dead terminals:
# `wait after command` is FALSE so a window vanishes when its process exits.
# The window id is tracked in /tmp/wwdos_ghostty_win so the next relaunch can
# close a hung window even if auto-close failed.
set -u
WIN_FILE=/tmp/wwdos_ghostty_win

pkill -f "build/app/wwdos" 2>/dev/null
sleep 1

# Close the previously tracked window if it still exists (hung/stray)
if [ -f "$WIN_FILE" ]; then
  OLD=$(cat "$WIN_FILE")
  osascript -e "tell application \"Ghostty\" to close (first window whose id is $OLD)" 2>/dev/null
  rm -f "$WIN_FILE"
fi

NEW_ID=$(osascript <<'EOF'
tell application "Ghostty"
    set cfg to new surface configuration
    set command of cfg to "/Users/james/Repos/wibandwob-dos-tvision/scripts/launch_wwdos_ghostty.sh"
    set wait after command of cfg to false
    set w to new window with configuration cfg
    activate window w
    return id of w
end tell
EOF
)
echo "$NEW_ID" > "$WIN_FILE"
echo "wwdos relaunched in Ghostty window $NEW_ID"
