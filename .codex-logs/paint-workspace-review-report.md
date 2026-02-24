# Paint Window + Workspace Management Review (Spike Branch)

## 1. CRITICAL bugs (will crash or corrupt)

- None found in the reviewed changes.
- I did not find a proven double-free / use-after-free in the new `TPaintWindow::buildLayout()` child rebuild path or in the `TManageWorkspacesDialog` modal loop as written.

## 2. MODERATE bugs (wrong behaviour, visual glitch)

- `parseWorkspaceWindows()` does not match the workspace JSON schema for window bounds, so previews will render incorrect window positions/sizes (usually default `0,0,20x10` stacked boxes).
  - Parser reads flat keys `x/y/w/h` from each window object: `app/test_pattern_app.cpp:4141`, `app/test_pattern_app.cpp:4142`, `app/test_pattern_app.cpp:4143`, `app/test_pattern_app.cpp:4144`
  - Saved workspaces store bounds under nested `"bounds"`: `app/test_pattern_app.cpp:3981`
  - Impact: `TWorkspacePreview` is mostly wrong even for valid workspaces.

- `parseWorkspaceWindows()` scans for `{...}` objects past the end of the `"windows"` array, so it can parse unrelated later objects (e.g. top-level objects after the array) as fake preview windows.
  - Starts at array but never finds/stops at the matching `]`: `app/test_pattern_app.cpp:4117`, `app/test_pattern_app.cpp:4120`, `app/test_pattern_app.cpp:4122`, `app/test_pattern_app.cpp:4147`
  - Impact: preview can include garbage rectangles depending on file layout.

- `parseWorkspaceWindows()` brace matching ignores quoted strings when finding `realEnd`, so titles/props containing `{` or `}` can break object slicing.
  - Raw depth scan counts braces without string/escape handling: `app/test_pattern_app.cpp:4130`, `app/test_pattern_app.cpp:4132`, `app/test_pattern_app.cpp:4133`, `app/test_pattern_app.cpp:4134`
  - Impact: malformed previews or dropped windows for otherwise valid JSON with brace characters in strings.

- Paint window resize rebuild discards user canvas contents and UI state.
  - `changeBounds()` always calls `buildLayout()`: `app/paint/paint_window.cpp:129`, `app/paint/paint_window.cpp:133`
  - `buildLayout()` destroys all child views and creates a new `TPaintCanvasView`: `app/paint/paint_window.cpp:57`, `app/paint/paint_window.cpp:59`, `app/paint/paint_window.cpp:106`
  - Impact: resizing the paint window wipes the drawing (and resets selection/tool panel state tied to the old canvas pointer).

- Composite draw path still has a blank-render gap: pixel data is hidden in `Text` mode when no `textChar` is present.
  - `pixelMode == Text` blank branch runs before pixel-layer checks: `app/paint/paint_canvas.cpp:144`, `app/paint/paint_canvas.cpp:146`
  - This contradicts the “text wins, then pixel data” composite intent and can show blank cells even when `u/l` or `qMask` data exists.

- Command ID collisions with terminal window commands (`500-502`) can cause cross-feature command confusion in shared command routing.
  - Paint uses `500/501/502`: `app/paint/paint_window.cpp:28`, `app/paint/paint_window.cpp:29`, `app/paint/paint_window.cpp:30`
  - TVTerm already reserves same IDs and explicitly says they must not collide: `app/tvterm_view.cpp:18`, `app/tvterm_view.cpp:22`, `app/tvterm_view.cpp:23`, `app/tvterm_view.cpp:24`
  - Impact depends on routing path, but this is a real namespace collision in the app.

## 3. MINOR issues (style, defensive coding)

- `parseWorkspaceWindows()` parses `canvasW/canvasH` but the result is effectively unused for preview scaling.
  - Parser reads `"canvas"` dimensions (which current saved schema does not write): `app/test_pattern_app.cpp:4100`, `app/test_pattern_app.cpp:4103`, `app/test_pattern_app.cpp:4107`, `app/test_pattern_app.cpp:4108`
  - `updatePreview()` only passes `wins`, so `TWorkspacePreview::setWindows()` falls back to `80x24`: `app/test_pattern_app.cpp:4265`, `app/test_pattern_app.cpp:4266`, `app/test_pattern_app.cpp:4159`

- Filename sanitization uses `std::isalnum(char)` on potentially signed `char`, which is undefined behavior for non-ASCII bytes.
  - `saveWorkspaceAs()`: `app/test_pattern_app.cpp:4061`, `app/test_pattern_app.cpp:4062`
  - Rename path: `app/test_pattern_app.cpp:4385`, `app/test_pattern_app.cpp:4386`
  - Cast to `unsigned char` before `std::isalnum` for safety.

- No null-terminator bug found in `Copy as Text` clipboard path.
  - `TPaintWindow` passes explicit length with `TStringView`: `app/paint/paint_window.cpp:161`
  - `TClipboard::setText` copies and appends `\0` via `newStr(TStringView)`: `vendor/tvision/source/tvision/tclipbrd.cpp:33`, `vendor/tvision/source/tvision/newstr.cpp:24`, `vendor/tvision/source/tvision/newstr.cpp:27`
  - Possible residual portability issue is encoding (UTF-8 block glyphs vs platform clipboard expectations), not termination.

- `TManageWorkspacesDialog` modal loop pattern looks ownership-safe in this review, but it is non-standard enough that a brief comment would help future maintainers.
  - `endModal()` in dialog event handler: `app/test_pattern_app.cpp:4294`
  - Reuse same dialog across repeated `execView(dlg)` calls: `app/test_pattern_app.cpp:4356`
  - Destroyed on all observed exits: `app/test_pattern_app.cpp:4363`, `app/test_pattern_app.cpp:4411`, `app/test_pattern_app.cpp:4425`

## 4. Summary verdict

- No immediate crash/corruption bug was proven in the reviewed code paths.
- The biggest regressions are functional: workspace preview parsing is incompatible/fragile (`bounds` schema mismatch + array/object scanning bugs), paint resize wipes canvas state, and the paint draw path still blanks pixel data in `Text` mode.
- I also recommend fixing the command ID collision with `tvterm` before these features are used together.
