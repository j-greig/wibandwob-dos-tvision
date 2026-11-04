#!/bin/sh
# The program of a WIBWOB-DOS terminal window opened with `open_terminal exec=<cmd>`.
# The app sets SHELL to this file and passes the command in WWDOS_TERM_EXEC, so the
# window runs <cmd> with no prompt, no echo and no cursor. SHELL goes back to the
# user's shell first, so programs started inside (an inner WIBWOB-DOS and its own
# terminals) get a normal shell. WWDOS_TERM_HOLD=1 keeps the last screen after <cmd>
# exits, until the window is closed.
#
# HUP, INT, TERM, QUIT and PIPE go back to their default action before <cmd> runs: the
# app can be started with SIGHUP ignored, children inherit that, and then closing the
# window (which sends SIGHUP) would leave <cmd> running.
cmd=$WWDOS_TERM_EXEC
hold=$WWDOS_TERM_HOLD
# The window kills this process group when it closes; it finds us by this file.
[ -n "$WWDOS_TERM_PIDFILE" ] && echo $$ > "$WWDOS_TERM_PIDFILE"
SHELL=${WWDOS_TERM_SHELL:-/bin/zsh}
export SHELL
unset WWDOS_TERM_EXEC WWDOS_TERM_HOLD WWDOS_TERM_SHELL WWDOS_TERM_PIDFILE
stty -echo 2>/dev/null
printf '\033[?25l'
run='$SIG{$_}="DEFAULT" for qw(HUP INT TERM QUIT PIPE); exec "/bin/sh", "-c", $ARGV[0] or exit 127'
if [ "$hold" = 1 ]; then
  perl -e "$run" "$cmd"
  printf '\033[?25l'
  exec perl -e '$SIG{$_}="DEFAULT" for qw(HUP INT TERM QUIT); sleep'
fi
exec perl -e "$run" "$cmd"
