---
title: "File > Recent Workspaces submenu"
status: done
created: 2026-02-24
github_issue: "—"
PR: "—"
---

# File > Recent Workspaces

Show last 5 workspace files as a submenu under File, so the user can restore
a previous layout with one click instead of using the file dialog.

## What exists

- `workspaces/` dir with timestamped `.json` files (e.g. `last_workspace_260224_0530.json`)
- `cmSaveWorkspace` / `cmOpenWorkspace` commands
- `loadWorkspaceFromFile(path)` loads any workspace JSON
- `initMenuBar()` is static, built once at startup

## Implementation

### 1. Scan at startup

In `TTestPatternApp` constructor (after base init), scan `workspaces/*.json`,
sort by mtime descending, keep top 5 paths in a `std::vector<std::string>`.

```cpp
std::vector<std::string> recentWorkspaces_;  // populated at startup
static constexpr int kMaxRecent = 5;
```

### 2. Build submenu dynamically in initMenuBar

`initMenuBar` is static but we can use a file-scoped vector populated before
the constructor calls `TProgInit`. Or simpler: scan in `initMenuBar` itself
(it runs once, filesystem scan is cheap).

```cpp
// Inside initMenuBar, before building the TSubMenu:
auto recents = scanRecentWorkspaces("workspaces/", 5);
TMenuItem* recentItems = nullptr;
for (int i = recents.size() - 1; i >= 0; --i) {
    // Show just the filename, not full path
    recentItems = new TMenuItem(recents[i].name.c_str(),
        cmRecentWorkspace + i, kbNoKey, hcNoContext, nullptr, recentItems);
}
```

Then in the File menu:
```
Save Workspace     Ctrl+S
Open Workspace...
Recent ▶  last_workspace_260224_0530
          last_workspace_260223_1851
          last_workspace_260223_0938
          ...
─────────
Exit               Alt+X
```

### 3. Command constants

```cpp
const ushort cmRecentWorkspace = 275;  // 275..279 for 5 slots
```

### 4. Handle in app handleEvent

```cpp
if (cmd >= cmRecentWorkspace && cmd < cmRecentWorkspace + kMaxRecent) {
    int idx = cmd - cmRecentWorkspace;
    if (idx < recentWorkspaces_.size())
        loadWorkspaceFromFile(recentWorkspaces_[idx]);
}
```

### 5. Refresh on save

After each `saveWorkspace()`, re-scan and update `recentWorkspaces_` vector.
The menu text won't update (TV menus are static) but the command handler uses
the vector, so new saves are available if the user knows the slot. For full
dynamic menu refresh, would need to destroy+rebuild the menu bar — doable but
heavier. **v1: don't refresh menu, just the vector. v2: rebuild menu on save.**

## Key files to edit

| File | Change |
|------|--------|
| `app/test_pattern_app.cpp` | Add `recentWorkspaces_`, scan helper, `cmRecentWorkspace` handler, modify `initMenuBar` |

## Acceptance criteria

- **AC-1:** File menu shows "Recent ▶" submenu with up to 5 entries
  - Test: start app with ≥5 workspace files → menu shows 5 items
- **AC-2:** Clicking a recent workspace loads it
  - Test: select item → workspace restores (windows, desktop state)
- **AC-3:** Empty workspaces dir → "Recent" submenu absent or shows "(none)"
  - Test: start with no workspace files → no crash, graceful

## Effort

~1.5h. Straightforward TV menu construction + filesystem scan.
