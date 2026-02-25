# E015 Live Test Plan

**Goal**: Two browser tabs → same URL → each gets own TUI + identity → chat relays between them.

**One URL, two tabs** = two independent users. ttyd forks a fresh `wwdos`+bridge per connection.

Legend: 🤖 = I run this / 👤 = you do this in browser or terminal

---

## Phase 1 — Sprite creation

- [ ] 🤖 Create Sprite: `sprite create wibwob-dos`
- [ ] 🤖 Verify it exists: `sprite list`

---

## Phase 2 — Get code onto Sprite

The repo is private. Three options in order of preference:

- [ ] 🤖 **Option A** (if Sprites GitHub app is linked): `sprite exec -- git clone https://github.com/j-greig/wibandwob-dos /home/sprite/app`
- [ ] 🤖 **Option B** (GitHub token): `sprite exec -- git clone https://<TOKEN>@github.com/j-greig/wibandwob-dos /home/sprite/app`
- [ ] 🤖 **Option C** (copy key files directly via exec): upload binaries + scripts

---

## Phase 3 — Provision

- [ ] 🤖 Install system deps: `sprite exec -- apt-get install -y ttyd cmake build-essential git libncurses-dev`
- [ ] 🤖 Build wwdos (sprites preset or Release): cmake + make
- [ ] 🤖 Install Python deps: `sprite exec -- bash -c "cd /home/sprite/app && uv sync"`
- [ ] 🤖 Check RAM headroom: `sprite exec -- free -m` → note total / per-user budget (~35 MB each)
- [ ] 🤖 Verify binary runs: `sprite exec -- /home/sprite/app/build-sprites/app/wwdos --help 2>&1 | head -3` (or just check exit isn't segfault)

---

## Phase 4 — Wire services + expose URL

- [ ] 🤖 Register ttyd as a Service (auto-restarts on wake)
- [ ] 🤖 Make URL public: `sprite url update --auth public`
- [ ] 🤖 Get URL: `sprite url` → should be `https://wibwob-dos-james-greig.sprites.app`
- [ ] 🤖 Verify ttyd responds: `curl -si https://wibwob-dos-james-greig.sprites.app | head -5` → expect `200 OK` with xterm.js page

---

## Phase 5 — Two-user browser test (👤 you do this)

> Open two **separate browser windows** (not tabs in same window — some browsers share WebSocket state).
> Or: one normal window + one incognito window.

- [ ] 👤 Open `https://wibwob-dos-james-greig.sprites.app` in **Window 1**
  - Expect: WibWob-DOS TUI loads in browser, cursor visible, `> _` input bar at bottom
  - Expect: Presence sidebar shows `• <your-name> (me)` + `1 in room`

- [ ] 👤 Open same URL in **Window 2** (separate window or incognito)
  - Expect: second TUI loads with **different** adjective-animal name
  - Expect: both windows now show `2 in room`
  - Expect: both sidebars show both names, yours labelled `(me)` in each

- [ ] 👤 **In Window 1**: type a message in `> _` bar, press Enter
  - Expect: message appears in Window 1 with `[HH:MM] me` label
  - Expect: message appears in Window 2 with `[HH:MM] <name-from-W1>` label

- [ ] 👤 **In Window 2**: type a reply, press Enter
  - Expect: reply appears in Window 2 with `[HH:MM] me` label
  - Expect: reply appears in Window 1 with `[HH:MM] <name-from-W2>` label

- [ ] 👤 **Close Window 2** (close the tab/window entirely)
  - Expect: Window 1 presence strip updates to `1 in room`

---

## Phase 6 — Verify no zombie bridges (🤖 I check this)

- [ ] 🤖 After Window 2 closed: `sprite exec -- ps aux | grep partykit_bridge | grep -v grep`
  - Expect: **0 bridge processes** for the closed session (F04 fix working)
  - Expect: **1 bridge process** still running for Window 1's session

---

## Phase 7 — Idle/wake test (👤 you do this)

- [ ] 👤 Close all windows. Wait ~2 minutes.
- [ ] 👤 Reopen the URL
  - Expect: Sprite wakes, ttyd restarts (Service), TUI loads fresh
  - May take 2–5 seconds on first request after sleep

---

## Pass criteria

| Check | Expected | Pass? |
|-------|----------|-------|
| Two browser windows → different names | amber-gnu ≠ sage-jay | |
| Both show `2 in room` | Presence strip | |
| Message W1 → appears W2 | Chat relay | |
| Message W2 → appears W1 | Bidirectional | |
| Close W2 → W1 shows `1 in room` | Presence leave | |
| No zombie bridges after W2 close | `ps aux` = 0 stale | |
| URL wakes after idle | Fresh TUI loads | |

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
