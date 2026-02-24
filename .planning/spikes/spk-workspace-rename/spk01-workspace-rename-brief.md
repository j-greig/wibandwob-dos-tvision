# SPK01 — Workspace Rename / Save-As

Status: in-progress  
GitHub issue: —  
PR: —

---

## Context

Workspaces can be saved and loaded but currently have no rename capability. The name is set once at save time and is immutable. This spike explores how and when workspace names should be editable.

## Core Tension: Document vs Snapshot

### Document model (VS Code style)
- You're always "in" a workspace
- Title bar shows `WibWob-DOS — My Cool Layout`
- Save = overwrites current, Save As = new name
- Rename is obvious: editing the active thing's name
- **Problem:** feels heavy for a TUI. When you close 3 windows and open 5 new ones, are you still "in" the same workspace? At what point has it drifted into something else?

### Snapshot model (browser bookmarks style)
- Workspaces are saved states you can jump back to
- You're not "in" one — you saved one 20 minutes ago and have changed things since
- Rename happens in a Manage Workspaces dialog (list curation)
- **Advantage:** more honest about how people actually use it — especially when IPC/scripts spawn 80 windows and the state bears no relation to what was saved

### Hybrid model (recommended direction)
- When you **load** a workspace, you're loosely "in" it — show the name in status bar (dimmed/italic to signal "approximate")
- Any window change makes it `My Layout *` (dirty flag, like unsaved file)
- **File → Save Workspace** overwrites if you loaded one, prompts name if fresh
- **File → Save Workspace As...** always prompts for a name
- **File → Manage Workspaces...** opens a TListBox dialog with rename/delete/load buttons

## Where Could Rename Live?

| Surface | When active | Feel |
|---------|-------------|------|
| Save As... dialog | At save time | Natural, low friction, MVP |
| Manage Workspaces dialog | Deliberate curation | Power user, clean |
| Right-click workspace in File menu | Browsing workspace submenu | Discoverable |
| Status bar double-click | Anytime (if workspace loaded) | Slick but hidden |
| App title bar | If workspace = document model | Too committal? |

## P1 / MVP: Save Workspace As...

The simplest first step:

- **File → Save Workspace As...** opens `inputBox()` with current name pre-filled (or blank if never saved)
- Saves workspace JSON to `~/.wibwob/workspaces/<name>.json` (or wherever they live now)
- If name matches existing workspace, confirm overwrite
- No rename per se — just save-as with a new name (rename = save-as + delete old, which the user can do manually)

### AC
- AC-1: File menu has "Save Workspace As..." item
  - Test: Open menu, verify item present
- AC-2: Dialog pre-fills current workspace name if one was loaded
  - Test: Load workspace "foo", Save As shows "foo" in input field
- AC-3: Saving with new name creates new workspace file
  - Test: Save As "bar", verify file created, verify appears in load menu
- AC-4: Saving with existing name prompts overwrite confirmation
  - Test: Save As with name of existing workspace, verify confirm dialog

## P2: Manage Workspaces Dialog

A `TDialog` with a `TListBox` of all saved workspaces:

- **Load** button (or double-click) — loads selected workspace
- **Rename** button — opens `inputBox` to rename, updates filename
- **Delete** button — confirms then removes workspace file
- **Close** button

Could be reached from **File → Manage Workspaces...** or a keyboard shortcut.

### AC
- AC-5: Dialog lists all saved workspaces
  - Test: Save 3 workspaces, open dialog, verify all 3 shown
- AC-6: Rename updates both filename and display name
  - Test: Rename "foo" to "bar", verify old file gone, new file present
- AC-7: Delete removes workspace with confirmation
  - Test: Delete workspace, confirm dialog, verify file removed
- AC-8: Load closes dialog and restores workspace
  - Test: Load from dialog, verify windows restored

## P3: Status Bar Workspace Indicator

- Status bar shows `[workspace: My Layout]` when a workspace was loaded
- Changes to `[workspace: My Layout *]` on any window create/close/move/resize
- Double-click status bar workspace name → opens Save As dialog
- Fades to `[no workspace]` if too many changes (heuristic TBD)

### Open Questions
- How many changes before we stop showing the loaded workspace name?
- Should the dirty flag track specific window changes or just "anything changed"?
- Is the status bar indicator worth the complexity or is it cosmetic noise?

## P4: Right-Click in Workspace Menu

If the File menu has a workspace submenu (e.g. recent workspaces list):

- Right-click on a workspace name → context menu with Rename / Delete
- Requires tvision menu customisation (may not be trivial)

### Open Questions
- Does tvision support right-click in menus? (Probably not natively)
- Might need a custom TMenuBox subclass

## Design Decisions

1. **Start with snapshot model** — don't treat workspace as a document you're always "in". The document model gets weird when IPC scripts or agents reshape the desktop.
2. **Save As is the MVP** — gives naming and implicit rename (save-as new name). Zero new concepts.
3. **Manage dialog is the P2** — proper curation for power users who accumulate many workspaces.
4. **Status bar indicator is cosmetic P3** — nice-to-have, not essential.
5. **Workspace file format**: whatever exists now, just need to store the name as metadata inside the JSON (not just the filename).

## Related
- `.planning/spikes/spk-file-menu-show-recent-workspaces-as-nested-menu-maybe-last-5/` — recent workspaces in File menu (feeds into P4 right-click rename)
