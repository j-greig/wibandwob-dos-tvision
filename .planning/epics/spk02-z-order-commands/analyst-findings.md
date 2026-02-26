# Z-Order Analyst Findings — Implementation Brief

## Exact changes needed

### 1. app/wwdos_app.cpp line ~3199: Fix focus_window

BEFORE:
```cpp
app.deskTop->setCurrent(w, TDeskTop::normalSelect);
```

AFTER:
```cpp
w->select();  // raises to front AND focuses (uses makeFirst -> putInFrontOf)
```

This is already done.

### 2. app/wwdos_app.cpp: Add raise_window and lower_window functions

Add after api_focus_window (around line 3201):

```cpp
std::string api_raise_window(TWwdosApp& app, const std::string& id) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "{\"error\":\"Window not found\"}";
    w->select();
    return "{\"success\":true}";
}

std::string api_lower_window(TWwdosApp& app, const std::string& id) {
    TWindow* w = app.findWindowById(id);
    if (!w) return "{\"error\":\"Window not found\"}";
    // putInFrontOf(background) sends to back of z-stack but above wallpaper
    if (app.deskTop->background) {
        w->putInFrontOf(app.deskTop->background);
    }
    return "{\"success\":true}";
}
```

Add friend declarations near line 914 (where api_focus_window friend is):
```cpp
friend std::string api_raise_window(TWwdosApp&, const std::string&);
friend std::string api_lower_window(TWwdosApp&, const std::string&);
```

### 3. app/wwdos_app.cpp api_get_state(): Add z and focused fields

Near line 3099-3106 where window JSON is built, the current code emits id/type/x/y/w/h/title/path.

Need to:
- Track a z-index counter during desktop child traversal (0 = frontmost)
- Compare each window against deskTop->current to set focused
- Add to JSON: `"z":N,"focused":true/false`

### 4. app/command_registry.cpp: Register new commands

Add capability declarations (near line 103-106 where focus_window is):
```cpp
{"raise_window", "Bring window to front of z-order and focus it (id param)", true},
{"lower_window", "Send window to back of z-order (id param)", true},
```

Update focus_window description to: "Bring window to front and focus (id param)" 

Add dispatch handlers (near line 709-718 where focus_window dispatch is):
```cpp
if (name == "raise_window") {
    auto it = kv.find("id");
    if (it == kv.end()) return "err missing id";
    return api_raise_window(app, it->second);
}
if (name == "lower_window") {
    auto it = kv.find("id");
    if (it == kv.end()) return "err missing id";
    return api_lower_window(app, it->second);
}
```

### 5. tools/api_server/controller.py lines 203-204: Use real z and focused

BEFORE (hardcoded):
```python
z=0,
focused=False,
```

AFTER (parse from C++ response):
```python
z=int(ipc_win.get("z", 0)),
focused=bool(ipc_win.get("focused", False)),
```

## What NOT to touch
- Workspace save/restore — out of scope
- api_ipc.cpp — commands go through command_registry dispatch, not separate IPC handlers
- MCP layer — picks up new commands automatically via tui_list_commands
- Tests — add later, not in this implementation pass

## Build command
```bash
cmake --build ./build --target wwdos -j$(sysctl -n hw.ncpu)
```
