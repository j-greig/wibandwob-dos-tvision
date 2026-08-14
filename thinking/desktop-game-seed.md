# Desktop-game seed (wwdos as playable dungeon)

**tl;dr**: wwdos already has game bones — clickable disks, TV channel, rogue/snake/quadra views, workspaces-as-savegames, Scramble NPC, chat parser, IPC as dungeon-master API. Missing piece is *consequence*: state shared between windows. Seed captured 2026-08-13 from Zilla's "clickable states, typing bits, moving or interlinked windows... interfadating".

## What already exists (no new engine needed)

| Game primitive | Already in wwdos |
|---|---|
| Boot/launch verb | Disk Library double-click |
| Rooms / levels | Workspaces (positions + props round-trip) |
| Savegames | `save_workspace` / `open_workspace` |
| Player avatar | rogue_view `@`, snake, quadra |
| NPC | Scramble overlay (+ her moods) |
| Text parser / typing bits | chat window, room_chat, text_editor |
| Cutscenes | SHADER.SYS, TUIFORGE.TV |
| Dungeon master | IPC :8089 — an LLM already moves every window (chat MCP bridge) |
| Game art corpus | zzt-suite renders = screenshots of the game that doesn't exist yet |

## The one missing mechanic

**Interlinked window state.** A tiny shared blackboard (key/value in the app,
exposed via IPC + readable by views): pick up 🗝 in the rogue window →
`state.key=1` → a locked disk in the library becomes bootable → boots a
tuiforge render that contains the next clue. Windows become rooms with
edges; closing/moving a window can BE a mechanic.

## Candidate first playable (small)

"THE MISSING 9" — already themed in the corpus. 5 beats:
1. Disk library shows a corrupted disk (locked, label glitched)
2. TV channel occasionally flashes a render containing a code
3. Typing the code into a chat/primer window sets blackboard state
4. Corrupted disk unlocks, boots a zzt-suite board
5. rogue-style @ walks it; reaching the 9 saves a trophy workspace

## Non-goals (for now)
- No new engine, no game loop rewrite — everything rides existing views + IPC
- No realtime physics; TVision timers are enough (see TUIFORGE.TV pattern)

## Next motion when picked up
1. Blackboard: `std::map<std::string,std::string>` in app + `game_set`/`game_get` IPC commands + broadcast on change
2. DiskDef gains optional `locked_until` blackboard key
3. One TV station that injects a code render on a schedule
