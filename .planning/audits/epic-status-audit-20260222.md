# Epic Status Audit — 2026-02-22

Snapshot of all epics from E004 onwards, taken after merging `spike/spk-auth-unification` (39 commits) to main.

## Status Summary

| Epic | Title | Status | Notes |
|------|-------|--------|-------|
| E001 | Command Parity Refactor | done | |
| E002 | Browser & State Export | done | |
| E003 | Dark Pastel Theme | done | |
| E004 | Browser Rendering Reliability | **dropped** | Superseded by later work |
| E005 | Theme Runtime Wiring | **not-started** | Hot-swap themes at runtime. No progress. |
| E006 | Scramble TUI Presence | **in-progress** | ~90% done. F01–F03 shipped. F04 (error/state trigger reactions) and F05 (ask/poke/feed/mood API commands) still open. |
| E007 | Browser Hosted Deployment | done | |
| E008 | Multiplayer PartyKit | **in-progress** | WebSocket relay. Depends on E007 (done). Stalled. |
| E009 | Menu System Redesign | done | |
| E010 | Paint Canvas Integration | done | |
| E011 | Desktop Shell | **not-started** | Icons, folders, trash, app launcher enhancements. GH #78. |
| E012 | ASCII Gallery & Wib Vision | **not-started** (component built) | Gallery browser shipped in spike branch (6 tabs, search, preview scroll, API/MCP). Vision doc + layout spec written. Creative layout/presentation system not started. |
| E013 | Workspace Save/Restore Parity | **not-started** | Filed today. Props serialisation, surface parity audit, checklist-as-code. 6 features, 16 ACs. |

## EPIC_STATUS.md Drift

The register at `.planning/epics/EPIC_STATUS.md` is stale:
- Missing E011, E012, E013 entirely
- E004 listed as `not-started` but brief says `dropped`
- E009 listed as `in-progress` but brief says `done`
- E010 listed as `done` ✓ (correct)

## Findings from spike/spk-auth-unification

The spike went far beyond auth unification. Actual work delivered:

1. **Auth unification** (SPK-AUTH-01–05): AuthConfig singleton, dead code cleanup, status bar/dialog
2. **Async LLM**: SDK session non-blocking, streaming thread safety, Scramble async popen
3. **Scramble freeze fix**: root cause was inherited stdin fd (`</dev/null` fix)
4. **Terminal freeze fix**: `forkpty()` → `openpty()` + `vfork()` + `execve()`
5. **DRY refactors**: shared `wrapText()`, TScroller upgrade, debug cleanup
6. **ASCII Gallery**: 6-tab browser with search, preview scroll, API/MCP commands
7. **MCP tool consolidation**: 38 hardcoded tools → 2 generic (tui_list_commands + tui_menu_command)
8. **GET /commands endpoint**: agent command discovery
9. **vterm palette 0 override**: true black terminal background
10. **E013 planning**: workspace parity epic with surface audit feature

## Recommended Focus (next session)

### Quick wins
- **E006 closeout** (~30 min): Add remaining `scramble.ask`/`scramble.poke`/`scramble.feed`/`scramble.mood` API commands, or explicitly drop them and mark done.
- **EPIC_STATUS.md fix** (~5 min): Add E011–E013, fix E004/E009 status drift.

### High leverage
- **E013 F06 AC-13** (surface parity audit script): Map the full gap between menu commands, window types, command registry, API, MCP, workspace save/restore, and agent prompts. Informs all other E013 work.

### Creative / feature
- **E012** (ASCII Gallery vision): Gallery component is built. Remaining: creative layout system (honour intrinsic dimensions, breathing room, curated presentation), primer metadata, agent-driven gallery curation.

### Larger efforts
- **E013 F01–F05** (workspace serialisation): Fix getProps()/setProps() across all view classes, window type registry completeness, round-trip test. Medium-large.
- **E008** (multiplayer): Stalled. Needs dedicated focus session.
- **E011** (desktop shell): Large. Not urgent.
- **E005** (runtime themes): Not urgent.
