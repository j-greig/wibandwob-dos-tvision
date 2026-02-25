# E015 Live Test Plan

**Goal**: Two browser tabs → same URL → each gets own TUI + identity → chat relays between them.

**One URL, two tabs** = two independent users. ttyd forks a fresh `wwdos`+bridge per connection.

Legend: 🤖 = I run this / 👤 = you do this in browser or terminal

---

## Phase 1 — Sprite creation

- [x] 🤖 Create Sprite: `sprite create wibwob-dos`
- [x] 🤖 Verify it exists: `sprite list`

---

## Phase 2 — Get code onto Sprite

The repo is private. Three options in order of preference:

- [-] 🤖 **Option A** (if Sprites GitHub app is linked): `sprite exec -- git clone https://github.com/j-greig/wibandwob-dos /home/sprite/app`
- [x] 🤖 **Option B** (GitHub token): `sprite exec -- git clone https://<TOKEN>@github.com/j-greig/wibandwob-dos /home/sprite/app`
- [-] 🤖 **Option C** (copy key files directly via exec): upload binaries + scripts

---

## Phase 3 — Provision

- [x] 🤖 Install system deps: `sprite exec -- apt-get install -y ttyd cmake build-essential git libncurses-dev`
- [x] 🤖 Build wwdos (sprites preset or Release): cmake + make
- [x] 🤖 Install Python deps: `sprite exec -- pip install websockets`
- [x] 🤖 Check RAM headroom: `sprite exec -- free -m` → 16GB total, ~35 MB per user
- [x] 🤖 Verify binary runs: release binary built and tested

---

## Phase 4 — Wire services + expose URL

- [x] 🤖 Register ttyd as a Service (auto-restarts on wake)
- [x] 🤖 Make URL public: `sprite url update --auth public`
- [x] 🤖 Get URL: `https://wibwob-dos-bbxc2.sprites.app`
- [x] 🤖 Verify ttyd responds: TUI loads in browser

---

## Phase 5 — Two-user browser test (👤 you do this)

> Open two **separate browser windows** (not tabs in same window — some browsers share WebSocket state).
> Or: one normal window + one incognito window.

- [x] 👤 Open `https://wibwob-dos-bbxc2.sprites.app` in **Window 1**
  - ✅ WibWob-DOS TUI loads in browser with room chat + 3 primers
  - ✅ Presence sidebar shows `• <your-name> (me)` + `1 in room`

- [x] 👤 Open same URL in **Window 2** (separate window or incognito)
  - ✅ Second TUI loads with **different** adjective-animal name
  - ✅ Both windows show correct user count
  - ✅ Both sidebars show both names, yours labelled `(me)` in each

- [x] 👤 **In Window 1**: type a message in `> _` bar, press Enter
  - ✅ Message appears in Window 1 with `[HH:MM] me` label
  - ✅ Message appears in Window 2 with `[HH:MM] <name-from-W1>` label

- [x] 👤 **In Window 2**: type a reply, press Enter
  - ✅ Reply appears in Window 2 with `[HH:MM] me` label
  - ✅ Reply appears in Window 1 with `[HH:MM] <name-from-W2>` label

- [~] 👤 **Close Window 2** (close the tab/window entirely)
  - Expect: Window 1 presence strip updates to `1 in room`
  - ⚠️ Not yet verified — needs manual check

---

## Phase 6 — Verify no zombie bridges (🤖 I check this)

- [~] 🤖 After Window 2 closed: `sprite exec -- ps aux | grep partykit_bridge | grep -v grep`
  - Expect: **0 bridge processes** for the closed session (F04 fix working)
  - Expect: **1 bridge process** still running for Window 1's session
  - ⚠️ Not yet verified — needs manual check after tab close

---

## Phase 7 — Idle/wake test (👤 you do this)

- [ ] 👤 Close all windows. Wait ~2 minutes.
- [ ] 👤 Reopen the URL
  - Expect: Sprite wakes, ttyd restarts (Service), TUI loads fresh
  - May take 2–5 seconds on first request after sleep
  - Not yet tested

---

## Pass criteria

| Check | Expected | Pass? |
|-------|----------|-------|
| Two browser windows → different names | amber-gnu ≠ sage-jay | ✅ |
| Both show correct user count | Presence strip | ✅ |
| Message W1 → appears W2 | Chat relay | ✅ |
| Message W2 → appears W1 | Bidirectional | ✅ |
| /rename visible to other users | Sidebar updates | ✅ |
| Close W2 → W1 shows `1 in room` | Presence leave | ⏳ |
| No zombie bridges after W2 close | `ps aux` = 0 stale | ⏳ |
| URL wakes after idle | Fresh TUI loads | ⏳ |

---

## If something fails

| Symptom | Check |
|---------|-------|
| URL doesn't load | `sprite exec -- ps aux \| grep ttyd` — is ttyd running? |
| TUI garbled / no render | `sprite exec -- echo $TERM` — needs `xterm-256color` |
| Both windows same name | WIBWOB_INSTANCE not unique — check session script |
| `1 in room` even with 2 windows | Bridge not connecting — `sprite exec -- tail /tmp/wibwob_bridge_*.log` |
| Zombie bridges after close | F04 fix not deployed — check bridge version |
| Wake takes >30s | Cold start — normal, will improve on subsequent wakes |
