---
Status: draft
GitHub issue: —
PR: —
Epic: E016
Title: Shared Multi-User Experience
Depends: E008 (RoomChatView), E015 (Sprites hosting)
---

# E016 — Shared Multi-User Experience

> **TL;DR:** Two modes of multi-user wwdos. F01: each user has their
> own independent TUI but shares a chat room (what exists now, needs
> hardening). F02: all users share a single wwdos instance — same
> desktop, same windows, same cursor — like a shared OS with multiple
> terminals plugged in.

## Design Principle

**F01 is the foundation. F02 is the destination.**

F01 gives us a stable, working multi-user chat experience with zero sync
complexity. F02 is the ambitious goal: a single shared wwdos that
multiple people interact with simultaneously, like crowding around the
same computer.

---

## F01 — Independent TUIs + Multi-User Chat (Stabilise & Ship)

Each browser tab runs its own wwdos process. Window layout, navigation,
and content are per-user. Only chat messages and presence relay through
PartyKit. `WIBWOB_NO_STATE_SYNC=1` is the default.

### What's shared
- Chat messages (room chat view)
- Presence (who's online, adjective-animal names)

### What's per-user
- Everything else: window layout, editor content, gallery, games

### AC

- AC-01: Two browser tabs → two unique names, both show `2 in room`
  - Test: Open URL in normal + incognito, verify presence strip
- AC-02: Message typed in tab A appears in tab B within 2s
  - Test: Type "hello" in A, read in B, measure round-trip
- AC-03: Close tab B → tab A shows `1 in room` within 5s
  - Test: Close incognito, verify presence update in normal window
- AC-04: No cross-talk — opening/closing/resizing windows in A has
  zero effect on B's layout
  - Test: Open 5 windows in A, verify B's desktop unchanged
- AC-05: Typing in any view does not crash (menu hotkey null-submenu
  bug fixed)
  - Test: Type in room chat, text editor, Quadra — no segfault
- AC-06: Primers load from `modules-private/wibwob-primers/primers/`
  - Test: Open Text/Animation → select a primer → renders correctly
- AC-07: FIGlet renders with default font
  - Test: Open FIGlet window → "Hello" renders in ASCII art
- AC-08: Bridge exits cleanly when browser tab closed (no zombie)
  - Test: Close tab, wait 10s, `ps aux | grep bridge` shows no orphan

### Implementation notes

- Mostly done. Remaining work is acceptance testing + fixing edge cases.
- Known bugs to fix:
  - FIGlet font change locks the window (may be existing bug, not
    Sprites-specific)
  - Bridge zombie cleanup needs live verification
- `WIBWOB_NO_STATE_SYNC=1` set in `sprite-user-session.sh` as default

### Definition of done

All 8 ACs pass on live Sprite with 2 concurrent browser sessions.
No crashes, no zombies, no cross-talk.

---

## F02 — Shared OS (Single wwdos, Multiple Viewers)

All connected users see and interact with the same wwdos instance.
One process, one desktop, one set of windows. Multiple browser tabs
are terminals plugged into the same machine. Like VNC but native TUI.

### What's shared
- Everything: desktop, windows, content, cursor position, menus
- W&W conversations (everyone sees the same Scramble/chat)
- Gallery arrangements, editor documents, game state

### What's per-user
- Nothing — this is a truly shared experience
- (Stretch: cursor colour per user, so you can see who's doing what)

### Architecture

```
Browser A ──┐
Browser B ──┤──→ ttyd ──→ single wwdos process
Browser C ──┘              ↕
                       IPC socket
                           ↕
                    single bridge
                           ↕
                      PartyKit room
```

**Key difference from F01:** ttyd can multiplex multiple WebSocket
connections to the same underlying process using `tmux` or `script`
as the shared session host. All browsers see identical output because
they're reading from the same PTY.

### Implementation options (ranked by complexity)

#### Option A: tmux shared session (simplest)

```bash
# Session script creates or attaches to a shared tmux session
SHARED_SESSION="wwdos-shared"
if ! tmux has-session -t $SHARED_SESSION 2>/dev/null; then
  tmux new-session -d -s $SHARED_SESSION -x 120 -y 40 "$APP/build-sprites/app/wwdos"
fi
exec tmux attach-session -t $SHARED_SESSION
```

- Pros: 5 lines of code, battle-tested, handles resize gracefully
  (tmux uses smallest-client dimensions)
- Cons: All users share terminal size (smallest window wins);
  cursor is shared (users fight for control); no per-user identity
  unless bridge is enhanced
- Bridge: Single bridge instance (not per-user) connects to the
  one wwdos socket. All chat goes through one pipe.

#### Option B: ttyd `--once` + shared PTY (moderate)

Use ttyd's built-in session sharing. ttyd naturally broadcasts the
same PTY output to all connected WebSocket clients. The first
connection spawns the process; subsequent connections observe and
can also send input.

- Pros: No tmux dependency; native ttyd feature
- Cons: ttyd `--writable` lets all clients send input (no
  arbitration); resize is last-client-wins

#### Option C: Custom multiplexer (complex, V2)

Write a thin relay that reads from the single wwdos PTY and fans
output to all connected WebSocket clients. Input from any client
is forwarded to the PTY. Per-user cursor tracking via ANSI escape
interception.

- Pros: Full control, per-user cursors possible
- Cons: Significant engineering effort; diminishing returns vs
  Option A for MVP

### Recommended: Option A (tmux) for MVP

tmux is already installed on the Sprite. The shared-session pattern
is well-understood. Limitations (shared terminal size, input fighting)
are acceptable for the "crowding around one computer" experience and
can even be charming.

### AC

- AC-01: First browser opens → wwdos starts, shows desktop
  - Test: Open URL, verify TUI renders
- AC-02: Second browser opens → sees identical desktop (same windows,
  same content, same cursor position)
  - Test: Open second tab, verify pixel-identical output
- AC-03: User A opens a window → appears on User B's screen
  - Test: Open File menu in A, verify B sees it
- AC-04: User A types in editor → text appears on User B's screen
  - Test: Type "hello" in A's editor, verify B sees "hello"
- AC-05: User B closes a window → disappears from User A's screen
  - Test: Close window in B, verify A sees it gone
- AC-06: Chat works — messages appear for all viewers
  - Test: Open room chat, type message, verify all tabs see it
- AC-07: Window resize — smallest client dimensions used (tmux)
  - Test: Resize browser A to 80×24, verify B also shows 80×24
- AC-08: Last browser closes → wwdos keeps running (tmux detached)
  - Test: Close all tabs, reopen URL, verify state preserved

### Implementation notes

- Session script changes from "start new wwdos per connection" to
  "create-or-attach shared tmux session"
- Bridge becomes a singleton sidecar (started once with the tmux
  session, not per-connection)
- `WIBWOB_NO_STATE_SYNC` can be removed (there's only one instance,
  no sync needed — PartyKit is for external chat only)
- Presence becomes viewer count rather than unique identities
  (everyone IS the same terminal)
- Open question: how to show who's connected? Options:
  - Status bar: "3 viewers" (simple)
  - Per-user cursor colours via custom input relay (V2)
  - Chat room presence strip still works (bridge can report WebSocket
    connection count)

### Definition of done

All 8 ACs pass. Two browsers show identical TUI state. Actions in
either browser are visible in the other. wwdos survives all browsers
closing (tmux keeps session alive).

---

## Ordering

```
F01 (independent + chat) ← stabilise what exists
  └→ F02 (shared OS) ← build on stable foundation
```

F01 first because:
1. It's 90% done — just needs acceptance testing
2. It validates the Sprites hosting pipeline
3. Chat + presence relay work feeds into F02
4. F02 can reuse the same Sprite, URL, and service infrastructure

---

## Open Questions

1. **F02 input arbitration:** With tmux shared session, all users
   can type simultaneously. Is "whoever types last wins" acceptable
   for MVP, or do we need turn-taking / input locking?
2. **F02 terminal size:** tmux resizes to smallest client. Should
   we enforce a fixed size (e.g. 120×40) and let browsers scroll,
   or let it be dynamic?
3. **F02 persistence:** tmux session survives browser close. Should
   wwdos auto-exit after N minutes of no viewers? Or run forever?
4. **F02 + W&W agent:** In shared mode, W&W is truly shared — one
   conversation, all users see it. Does W&W need to know there are
   multiple humans? (Could be charming if W&W addresses the group.)
5. **Migration path:** Can users switch between F01 mode (independent)
   and F02 mode (shared) via a URL parameter or Sprite config?
   e.g. `?mode=shared` vs `?mode=independent`
