# Round 3 — Codex response

> NOTE: Codex rate-limited; written by Claude playing devil's advocate.

## Mostly agreed — one structural concern remains

Round 2 is solid. The architecture is correct. One thing to nail before writing
the brief: **the bridge startup race and cleanup are the only real risks.**

### Confirmed: ttyd default = per-connection spawn, no flags needed

ttyd forks the command for each new client connection by default. `--once`
means "exit ttyd itself after first client leaves" — that's wrong for us.
Without any flag: ttyd stays alive, spawns `sprite-user-session.sh` per
browser tab. Exactly what we want. Round 2's correction is right.

### The bridge/TUI lifecycle is the only thing that can go wrong

The wrapper script has a race:

```bash
(until [ -S /tmp/wibwob_${INST}.sock ]; do sleep 0.2; done
 uv run bridge.py) &
exec wwdos
```

**Risk 1 — bridge outlives TUI**: if wwdos crashes (segfault, bad terminal size
on connect), the socket disappears but the bridge loop `until [ -S ... ]` runs
forever spinning. Fix: add a timeout to the socket wait:

```bash
TIMEOUT=30
until [ -S /tmp/wibwob_${INST}.sock ] || [ $TIMEOUT -le 0 ]; do
  sleep 0.2; TIMEOUT=$((TIMEOUT-1))
done
[ -S /tmp/wibwob_${INST}.sock ] || exit 1
```

**Risk 2 — bridge hangs after disconnect**: when browser closes, PTY closes,
wwdos exits, socket removed. Does `partykit_bridge.py`'s poll loop exit
cleanly when the IPC socket is gone?

Quick check needed:

```bash
rg -n "sock|ConnectionRefused|FileNotFoundError|except" tools/room/partykit_bridge.py | head -20
```

If the bridge catches socket errors and exits → clean. If it retries forever →
zombie bridge processes accumulate on the Sprite eating RAM.

### The Sprite RAM question can be answered now

```bash
sprite exec -- free -m
```

Until that's run, "~35MB per user" is an estimate. Fine for the brief but flag
it as unverified.

### Room name: fixed env var is correct MVP choice

`WIBWOB_PARTYKIT_ROOM=rchat-live` baked into Sprite env. All users join the
same room always. If you want session-based rooms later (create a new room per
day / per event), that's a V2 feature. Not needed for MVP.

## AGREED on core architecture

- ✅ Single Sprite, ttyd default (per-connection fork), no `--once`
- ✅ `WIBWOB_INSTANCE=$$` → `/tmp/wibwob_$$.sock` (confirmed in source)
- ✅ `sprite-user-session.sh` wrapper: starts bridge after socket appears, runs TUI in foreground
- ✅ No API server in per-user path
- ✅ Fixed room name env var
- ✅ ~35MB per user (needs `sprite exec -- free -m` to confirm ceiling)
- ⚠️ Add bridge socket-wait timeout to wrapper script
- ⚠️ Verify bridge exits cleanly on socket-gone before deploying

## Ready to write the brief

These two risks are implementation-level, not architecture-level. The structure
is settled. Write the agreed brief and update `e015-epic-brief.md`.
