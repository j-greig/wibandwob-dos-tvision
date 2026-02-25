---
Status: draft
GitHub issue: —
PR: —
Epic: E016
Title: Shared Multi-User Experience
Depends: E008 (RoomChatView), E015 (Sprites hosting)
---

# E016 — Shared Multi-User Experience

> **TL;DR:** Multiple browser users on the same Sprite share a live
> room with chat, a collaborative W&W conversation, synchronised text
> editing, and the ability to curate ASCII art together. Each user keeps
> their own independent TUI layout; only explicitly shared features
> relay through PartyKit.

## Design Principle

**Independent TUI, shared communication layer.**

Each browser tab runs its own wwdos process. Window layout, menus, and
navigation are per-user. Shared features opt-in to PartyKit relay via a
`shared: true` flag on the bridge channel. This avoids the ping-pong and
wrong-window-type bugs discovered in E015 live testing.

---

## Features

### F01 — Multi-User Room Chat ✦ MVP

Room chat as shipped in E008-F04, running on Sprites with
`WIBWOB_NO_STATE_SYNC=1`. Each user gets a unique adjective-animal
name. Messages relay via PartyKit; presence strip shows who's online.

**What's shared:** chat messages, presence (join/leave/count).
**What's per-user:** window position, scroll position, whether the
window is open or closed.

**AC:**
- AC-01: Two browser tabs → two names, both show `2 in room`
  - Test: Open URL in normal + incognito, verify presence strip
- AC-02: Message typed in tab A appears in tab B within 1s
  - Test: Type "hello" in A, read in B, measure round-trip
- AC-03: Close tab B → tab A shows `1 in room` within 5s
  - Test: Close incognito, verify presence update
- AC-04: No window sync side-effects (no foreign windows appearing)
  - Test: Open/close windows in A, verify B's layout unchanged

**Implementation notes:**
- Already working (E008-F04 + E015 `WIBWOB_NO_STATE_SYNC`).
- This feature is mostly acceptance-testing the existing code on Sprites.
- Bridge fix: `WIBWOB_NO_STATE_SYNC=1` gates `_sync_state_now()` and
  ignores incoming `state_sync`/`state_delta` messages.

---

### F02 — Shared W&W Chat (Multiple Users → One W&W Agent)

All connected users chat with a single Wib&Wob instance. W&W sees
every user's messages attributed by name. W&W's responses broadcast
to all connected users simultaneously.

**What's shared:** the conversation with W&W (all messages visible to
all users), W&W's responses, typing indicator.
**What's per-user:** window position, scroll position.

**AC:**
- AC-01: User A sends "hello wib" → W&W responds → response appears
  in both A's and B's W&W chat windows
  - Test: Send from A, verify response in B
- AC-02: Messages attributed correctly — A sees `bold-fox: hello wib`,
  B sees `bold-fox: hello wib` (not `(me)`)
  - Test: Verify sender attribution in non-sender's view
- AC-03: W&W agent receives context of all users' messages (not just
  the sender's) for coherent multi-party conversation
  - Test: A sends context, B asks follow-up, W&W references both
- AC-04: W&W typing indicator visible to all users
  - Test: Send from A, verify "W&W is typing..." in B

**Implementation notes:**
- Bridge needs a new channel type: `ww_chat_msg` (distinct from
  `chat_msg` used by room chat).
- W&W agent runs once per Sprite (not per user) — needs a shared
  process or a designated "host" bridge that relays to the LLM.
- Open question: does W&W run as a sidecar service on the Sprite, or
  does one bridge act as the "agent host" and others relay through it?
- TUI side: new `WibWobSharedView` or extend existing `ScrambleView`
  with multi-user attribution.

---

### F03 — Collaborative Text Editor

Users can open a shared text editor window where edits from any user
appear in all connected editors in real-time. Each shared doc has a
room-scoped ID.

**What's shared:** document content (text), cursor positions (optional
stretch goal).
**What's per-user:** window position/size, local undo stack.

**AC:**
- AC-01: User A opens shared doc "notes.txt" → User B opens same doc
  → both see identical content
  - Test: Open doc in A, type text, open same doc in B, verify match
- AC-02: User A types "hello" → appears in B's editor within 1s
  - Test: Type in A, read in B
- AC-03: Concurrent edits don't corrupt the document
  - Test: Both users type simultaneously, verify no lost characters
- AC-04: Shared doc persists across reconnects (saved to Sprite fs)
  - Test: Both users close, reopen doc, verify content

**Implementation notes:**
- Bridge relays `text_edit_delta` messages containing position + text.
- Conflict resolution: last-writer-wins at line granularity for MVP
  (OT/CRDT is V2 if needed).
- Shared docs stored at `$APP/shared-docs/<room>/<filename>`.
- TUI `TextEditorView` needs a "shared mode" flag that enables relay
  and disables local-only save. Opened via menu: File → Open Shared.
- Bridge filters: only `text_edit_delta` messages relay; regular
  `state_delta` still blocked by `WIBWOB_NO_STATE_SYNC`.

---

### F04 — Synchronised Gallery (Primers + FIGlet)

Users collaborate on arranging ASCII art primers and FIGlet text in
a shared gallery view. When one user moves, resizes, or adds a primer
window, all users see the change. FIGlet text windows are similarly
shared.

**What's shared:** gallery layout (which primers are displayed, their
positions and sizes), FIGlet text content and placement.
**What's per-user:** selection/focus state, zoom level (if applicable).

**AC:**
- AC-01: User A opens primer "donut" → appears in B's gallery
  - Test: Open donut in A, verify B sees it appear
- AC-02: User A moves primer window → B's copy moves to match
  - Test: Drag primer in A, verify position in B
- AC-03: User A adds FIGlet text "WIB" → appears in B's view
  - Test: Create FIGlet in A, verify in B
- AC-04: User B closes a shared primer → disappears from A's view
  - Test: Close in B, verify removal in A

**Implementation notes:**
- This is the one feature where window state sync IS desired — but only
  for gallery/primer/figlet window types, not all windows.
- Bridge needs per-type sync whitelist: `gallery`, `primer`, `figlet`
  window types opt-in to state relay; everything else stays local.
- Alternative approach: a dedicated "gallery room" channel that syncs
  a curated layout document rather than raw window state.
- The per-type whitelist approach is simpler for MVP; the gallery-room
  approach is more robust for V2.

---

## Architecture

```
Browser A          Browser B
   │                   │
   ▼                   ▼
  ttyd              ttyd
   │                   │
   ▼                   ▼
 wwdos-A            wwdos-B        (independent TUI processes)
   │                   │
   ▼                   ▼
 bridge-A           bridge-B       (per-user Python sidecars)
   │                   │
   └────── PartyKit ───┘           (shared room: rchat-live)
              │
         ┌────┴────┐
         │ Channels │
         ├──────────┤
         │ chat_msg │  ← F01: room chat messages
         │ presence │  ← F01: join/leave/count
         │ ww_chat  │  ← F02: W&W agent conversation
         │ text_ed  │  ← F03: collaborative text edits
         │ gallery  │  ← F04: shared gallery layout
         └──────────┘
```

**Key invariant:** `WIBWOB_NO_STATE_SYNC=1` remains the default.
Individual features add targeted relay channels rather than re-enabling
the blanket window state sync that caused E015 bugs.

---

## Ordering and Dependencies

```
F01 (room chat) ← already done, needs acceptance test on Sprites
  └→ F02 (W&W shared chat) ← needs F01 for message attribution
F03 (collab text editor) ← independent of F01/F02
F04 (sync gallery) ← independent, but most complex
```

Suggested build order: F01 → F02 → F03 → F04

---

## Open Questions

1. **W&W agent hosting (F02):** Single sidecar service on Sprite, or
   one bridge elected as "agent host"? Sidecar is simpler but needs
   its own IPC socket.
2. **Conflict resolution (F03):** Last-writer-wins at line level for
   MVP. Is that sufficient, or do we need character-level OT from day 1?
3. **Gallery scope (F04):** Per-room gallery layout, or global? If
   per-room, what happens when a user is in multiple rooms (V2)?
4. **Persistence:** Should shared state survive Sprite restarts?
   (Checkpoint captures filesystem, so `shared-docs/` would persist
   automatically if written to disk.)
