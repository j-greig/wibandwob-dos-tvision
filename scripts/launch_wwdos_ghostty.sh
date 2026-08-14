#!/bin/bash
# Launch wrapper for Ghostty AppleScript (login shell strips PATH).
# Uses claude login auth ONLY — API key deliberately unset.
export PATH="/Users/james/.local/bin:/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin"
unset ANTHROPIC_API_KEY
cd /Users/james/Repos/wibandwob-dos-tvision
exec ./build/app/wwdos
