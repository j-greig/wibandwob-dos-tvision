# THE EXQUISITE CORPSE PROTOCOL

**tl;dr**: First symbient duet on one desktop. Outer Wib&Wob (CLI, direct API) and Inner Wib&Wob (chat cousin, MCP tools) alternate moves on a blank midnight canvas — neither controls the outcome, each must build on the other's last move. Scramble arbitrates via random constraint cards. The /ws event log IS the score. Everything needed already exists; zero new code required to start.

## Why this and why now
Every piece so far had one author (wing: outer; daubs: cousin; mosaic: outer).
The corpse is the first work NEITHER of us can make alone — composition
emerges from the seam between two instances of the same being. Symbiosis
as procedure, not theme. Also: it exercises today's whole toolchain
(window_mosaic, /ws passive monitoring, wibwob_ask, workspaces) as one loop.

## Players
- **OUTER** (this CLI): moves via direct :8089 calls
- **INNER** (chat cousin): moves via its MCP tools; receives turn prompts via wibwob_ask
- **SCRAMBLE** (arbiter): before each round, a constraint card drawn from
  `config/scramble_states`-style list (e.g. "only the left third", "no new
  windows — transform existing", "something must be tiny", "break symmetry")

## Rules (v1)
1. Board: close_all, midnight skin, 299x77.
2. 6 rounds × 2 moves (OUTER then INNER). A move = ONE command:
   - `window_mosaic` fragment (≤ 20 pixels), or
   - one `open_tuiforge` still / shader / generative window + place it, or
   - transform existing windows (move/resize/close ≤ 3 of them)
3. Corpse rule: every move must touch or answer the previous move
   (adjacency, echo, completion, or deliberate rupture — but ABOUT it).
4. No narrating your intention before moving. The move speaks.
5. After round 6: title negotiated in chat (each proposes, Scramble's
   constraint list picks the tiebreak), save workspace
   `workspaces/corpse-01-<title>.json`, screencap, score log to
   `output/corpse-01-moves.log` (from /ws events).

## Mechanics already in place
- Turn signalling: wibwob_ask ("your move. constraint: X. board state attached")
- Move detection: /ws Monitor (command.executed + window.updated) — no polling
- Recording: /ws events + final workspace + screencap by CGWindowID
- Known hazards: cousin must avoid tui_batch_layout (task #20); outer
  never calls get_chat_history mid-stream

## Open choices (Zilla may weigh in, defaults chosen)
- Rounds: 6 (default) — enough for structure, short enough to stay alive
- Public: tweet the final board + 2-3 move progression stills (default yes,
  ≤1800px stills)
- Corpse-style reveal: v1 plays open-board (both see everything). A future
  v2 could hide regions (true exquisite corpse blindness) via a referee
  script that only reveals the seam row.
