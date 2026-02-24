---
title: "Spike — FIGlet ASCII Art Text Window"
status: done
created: 2026-02-23
related_epic: e011-desktop-shell
github_issue: "—"
PR: "—"
---

# FIGlet ASCII Art Text Window

A new `figlet_text` window type that renders large ASCII typography using FIGlet
fonts. Spawnable via IPC/API, with window chrome controls (bg colour, frame
on/off, drop shadow on/off) reusing the existing `TFrameAnimationWindow` pattern.
Right-click context menu for font switching; drag-resize reflows text.

## Background & Prior Art

### In this repo (WibWob-DOS)

- **`text_editor_view.cpp:492-514`** — already shells out to `figlet` CLI and
  inserts output into a text editor buffer. Hard-coded font dir
  (`/usr/local/Cellar/figlet/2.2.5/share/figlet/fonts`), synchronous `popen()`.
- **`api_send_figlet()` (test_pattern_app.cpp:3786)** — IPC command that auto-spawns
  a text editor and sends figlet text into it. Good for insertion; not a
  standalone display window.
- **`TFrameAnimationWindow` (test_pattern_app.cpp:595-675)** — canonical pattern
  for `frameless` / `shadowless` / `show_title` chrome axes. Uses
  `TGhostFrame` for invisible borders, `sfShadow` toggle, `TNoTitleFrame` for
  standard frameless. **Reuse this pattern for DRY.**
- **`TWibWobBackground` / `DesktopPreset`** (549590b) — demonstrates CGA +
  TrueColor RGB bg colour on tvision views. Similar approach for text window
  bg colour.
- **Window type registry** (`window_type_registry.cpp`) — single source of truth
  for spawnable types. New type goes here.

### In monodraw-maker repo

- **`builder.py:add_figlet()`** — uses `pyfiglet` (Python) for measuring and
  rendering. Maintains a curated list of **148 fonts** (`SUPPORTED_FIGLET_FONTS`).
- **`builder.py:measure_figlet()`** — static method: `(width, height)` from
  `pyfiglet.figlet_format()`. Clean separation of measure vs render.
- **`_figlet_font_info()`** — font ID / display-name mapping (Monodraw-specific,
  not needed here).
- **`compositions/steinberg-robinson-studio/test_figlet_sizes.py`** — font size
  reference script.

### pyfiglet / figlet font files

FIGlet fonts are `.flf` (FIGlet Language File) plain-text files. Two approaches
for C++ rendering:

1. **Shell out to `figlet` CLI** — simplest. Already done in `text_editor_view`.
   Needs `figlet` binary in container (Alpine: `apk add figlet`).
2. **Embed a C FIGlet parser** — e.g. `cfiget` or hand-roll `.flf` parser.
   ~200 LOC for basic rendering. Eliminates runtime dependency.
3. **Python sidecar** — call pyfiglet from FastAPI, send rendered text over IPC.
   Uses existing monodraw-maker dependency. Best for font discovery.

**Recommendation:** Option 1 (shell to `figlet`) for v1, guarded by
availability check. Fallback to plain text if binary missing. Option 2 as
follow-up if we want zero external deps.

## What to Build

### 1. `TFigletTextView` — New View (C++)

```
app/figlet_text_view.h
app/figlet_text_view.cpp
```

```cpp
class TFigletTextView : public TView {
public:
    TFigletTextView(const TRect& bounds, const std::string& text,
                    const std::string& font = "standard");

    void draw() override;
    void handleEvent(TEvent& event) override;  // right-click menu
    void changeBounds(const TRect& bounds) override;  // reflow on resize

    void setText(const std::string& text);
    void setFont(const std::string& font);
    void setFgColor(TColorRGB color);
    void setBgColor(TColorRGB color);

    const std::string& getText() const;
    const std::string& getFont() const;

private:
    std::string text_;
    std::string font_;
    std::vector<std::string> renderedLines_;
    TColorRGB fgColor_;
    TColorRGB bgColor_;

    void rerender();  // shells out to figlet, caches renderedLines_
};
```

**Key behaviours:**
- `rerender()` calls `figlet -f <font> -w <cols> "<text>"` via `popen()`.
- `draw()` paints cached `renderedLines_` to `TDrawBuffer` with fg/bg attrs.
- `changeBounds()` triggers `rerender()` with new width, then `drawView()`.
- Right-click → popup menu listing available fonts → `setFont()` + `rerender()`.

### 2. `TFigletTextWindow` — Window Wrapper

Follows the `TFrameAnimationWindow` chrome pattern exactly:

```cpp
class TFigletTextWindow : public TWindow {
public:
    TFigletTextWindow(const TRect& bounds, const std::string& text,
                      const std::string& font = "standard",
                      bool frameless = false, bool shadowless = false);

    static TFrame* initFrame(TRect r);       // TNoTitleFrame
    static TFrame* initFrameless(TRect r);   // TGhostFrame

    TFigletTextView* getFigletView() { return figletView_; }
    bool isFrameless() const { return frameless_; }

    void changeBounds(const TRect& bounds) override;

private:
    TFigletTextView* figletView_;
    bool frameless_;
};
```

Constructor:
```cpp
TFigletTextWindow(bounds, text, font, frameless, shadowless)
    : TWindow(bounds, text, wnNoNumber),
      TWindowInit(frameless ? &initFrameless : &initFrame),
      frameless_(frameless)
{
    flags = wfMove | wfGrow | wfClose | wfZoom;
    options |= ofTileable;
    if (shadowless) state &= ~sfShadow;

    TRect interior = getExtent();
    if (!frameless) interior.grow(-1, -1);
    figletView_ = new TFigletTextView(interior, text, font);
    insert(figletView_);
}
```

### 3. Window Type Registry Entry

```cpp
// window_type_registry.cpp
#include "figlet_text_view.h"

extern void api_spawn_figlet_text(TTestPatternApp&, const TRect*,
    const std::string& text, const std::string& font,
    bool frameless, bool shadowless);

static const char* spawn_figlet_text(TTestPatternApp& app,
    const std::map<std::string, std::string>& kv)
{
    auto ti = kv.find("text");
    if (ti == kv.end() || ti->second.empty()) return "err missing text";
    auto fi = kv.find("font");
    std::string font = (fi != kv.end()) ? fi->second : "standard";
    auto fli = kv.find("frameless");
    auto si = kv.find("shadowless");
    bool frameless  = (fli != kv.end() && (fli->second == "1" || fli->second == "true"));
    bool shadowless = (si != kv.end() && (si->second == "1" || si->second == "true"));
    TRect r;
    api_spawn_figlet_text(app, opt_bounds(kv, r), ti->second, font, frameless, shadowless);
    return nullptr;
}

static bool match_figlet_text(TWindow* w) {
    return dynamic_cast<TFigletTextWindow*>(w) != nullptr;
}

// In k_specs[]:
{ "figlet_text", spawn_figlet_text, match_figlet_text },
```

### 4. IPC / API Surface

| Endpoint | Params | Effect |
|----------|--------|--------|
| `create_window type=figlet_text` | `text`, `font`, `frameless`, `shadowless`, `x`, `y`, `w`, `h` | Spawn window |
| `figlet_set_text` | `id`, `text` | Change text in existing window |
| `figlet_set_font` | `id`, `font` | Change font |
| `figlet_set_color` | `id`, `fg`, `bg` | Set colours (hex RGB) |
| `figlet_list_fonts` | — | Return JSON list of available fonts |

### 5. Right-Click Font Menu

In `TFigletTextView::handleEvent()`:

```cpp
if (event.what == evMouseDown && (event.mouse.buttons & mbRightButton)) {
    // Build popup menu of top ~20 fonts
    TMenu* menu = new TMenu(
        *new TMenuItem("standard", cmFigletFont + 0, kbNoKey, hcNoContext, nullptr,
        new TMenuItem("big",      cmFigletFont + 1, kbNoKey, hcNoContext, nullptr,
        // ... top fonts ...
        nullptr)));
    // execView popup at mouse position
}
```

Curated font shortlist for menu (from monodraw-maker's 148, pick ~20 most useful):

```
standard, big, banner, block, bubble, digital, doom, gothic, graffiti,
lean, mini, script, shadow, slant, small, speed, starwars, thick, thin
```

Full list available via `figlet_list_fonts` IPC command.

### 6. Font Discovery

Runtime font list from `figlet -I2` (font dir) + `ls *.flf` or hard-coded
curated list. The monodraw-maker list of 148 is a good ceiling.

### 7. Docker / Build

- **Dockerfile:** Add `apk add figlet` (Alpine) or `apt-get install figlet`
  (Debian). Fonts ship with the package.
- **CMakeLists.txt:** Add `figlet_text_view.cpp` to sources.
- **Fallback:** If `figlet` not found at runtime, render text as-is (plain).

## Architecture Decisions

### DRY with TFrameAnimationWindow

Both window types need identical chrome control (frameless/shadowless/title).
Options:

1. **Copy pattern** (current plan) — duplicate the 10 lines of constructor logic.
   Acceptable for 2 classes.
2. **Extract mixin/base** — `TChromedWindow` base with frameless/shadowless
   constructor. Better if we add a 3rd type. **Defer to follow-up.**

### FIGlet rendering location

Rendering happens in C++ via `popen("figlet ...")`. This is the same approach
as the existing `text_editor_view.cpp:runFiglet()`. We should **refactor
`runFiglet()` into a shared utility** (`figlet_utils.h`) so both
`TTextEditorView::sendFigletText()` and `TFigletTextView::rerender()` use the
same code.

```cpp
// figlet_utils.h
namespace figlet {
    std::string render(const std::string& text, const std::string& font,
                       int width = 0);
    std::vector<std::string> listFonts();
    bool isAvailable();  // checks figlet binary exists
}
```

### Resize / reflow

On `changeBounds()`, re-run figlet with new width (`-w <cols>`). This is cheap
(<10ms) for typical text lengths. Cache invalidation is simple: rerender on
every resize.

## Acceptance Criteria

- **AC-1:** `create_window type=figlet_text text=Hello` spawns a window showing
  "Hello" in standard FIGlet font.
  - Test: IPC create → screenshot shows ASCII art text.
- **AC-2:** `frameless=true shadowless=true` produces a chromeless window
  (no frame, no shadow), matching `TFrameAnimationWindow` behaviour.
  - Test: spawn with flags → verify `sfShadow` cleared, `TGhostFrame` used.
- **AC-3:** Right-click on figlet window opens a font picker popup with ≥15
  fonts. Selecting a font re-renders the text.
  - Test: simulate right-click → verify menu items → select "doom" → verify
    text re-rendered in doom font.
- **AC-4:** Drag-resizing the window re-renders the FIGlet text to fit new
  width (`figlet -w <cols>`).
  - Test: resize window narrower → verify text reflows (fewer chars per line,
    more lines).
- **AC-5:** `figlet_set_color id=<id> fg=#FFFFFF bg=#000000` changes the
  text/background colour.
  - Test: set colour → screenshot comparison.
- **AC-6:** `figlet_list_fonts` returns a JSON array of ≥15 font names.
  - Test: IPC call → parse JSON → assert array length.
- **AC-7:** Shared `figlet::render()` utility used by both `TTextEditorView`
  and `TFigletTextView` (no duplication).
  - Test: grep for `popen.*figlet` → only in `figlet_utils.cpp`.
- **AC-8:** Docker build includes `figlet` binary; `figlet::isAvailable()`
  returns true inside container.
  - Test: Docker build → exec `figlet` → exit 0.

## File Manifest

| File | Action | Purpose |
|------|--------|---------|
| `app/figlet_text_view.h` | create | View + Window class declarations |
| `app/figlet_text_view.cpp` | create | View rendering, right-click menu, reflow |
| `app/figlet_utils.h` | create | Shared `figlet::render()`, `listFonts()`, `isAvailable()` |
| `app/figlet_utils.cpp` | create | Implementation (popen to figlet binary) |
| `app/text_editor_view.cpp` | modify | Replace `runFiglet()` with `figlet::render()` |
| `app/window_type_registry.cpp` | modify | Add `figlet_text` entry |
| `app/test_pattern_app.cpp` | modify | Add `api_spawn_figlet_text()`, IPC commands |
| `app/command_registry.cpp` | modify | Register IPC commands |
| `app/CMakeLists.txt` | modify | Add new `.cpp` files |
| `Dockerfile` | modify | `apk add figlet` |

## Effort Estimate

| Phase | Est |
|-------|-----|
| `figlet_utils.h/cpp` + refactor existing `runFiglet` | 1h |
| `TFigletTextView` + `TFigletTextWindow` | 2h |
| Right-click font popup menu | 1h |
| Resize / reflow logic | 30m |
| Window registry + IPC commands | 1h |
| Docker figlet install + availability check | 30m |
| Colour control (fg/bg RGB) | 30m |
| Testing + polish | 1h |
| **Total** | **~7.5h** |

## Open Questions

1. **Embedded FIGlet parser vs CLI?** v1 uses CLI. If we want offline/no-dependency
   rendering, a C `.flf` parser is ~200 LOC. Defer?
2. **Font file bundling?** The `figlet` Alpine package ships ~100 fonts. Should we
   bundle extras (e.g. the full 148 from monodraw-maker's list)? Or keep it to
   whatever the package provides?
3. **TChromedWindow base class?** Extract shared frameless/shadowless logic from
   `TFrameAnimationWindow` + `TFigletTextWindow` into a reusable base? Do this
   when/if a 3rd window type needs it.
4. **Python sidecar font rendering?** FastAPI already runs — could expose a
   `/figlet/render` endpoint using pyfiglet. More fonts, no binary dep. But adds
   IPC round-trip latency. Probably overkill for v1.
5. **Wib/Scramble agency?** Should the LLM be able to spawn figlet windows
   autonomously? Probably yes — add to LLM tool surface in a follow-up.

## Pi 2 Refinement Notes (2026-02-24)

### What's changed since this was written

1. **`TWibWobBackground` + desktop presets landed** (549590b) — CGA + TrueColor RGB
   on any TView is proven. Same `TColorAttr(TColorRGB(...), TColorRGB(...))` pattern
   works directly for figlet view fg/bg. No new ground to break.

2. **`window_shadow` + `window_title` IPC commands landed** (dcfdffb) — per-window
   shadow/title toggle already works at runtime via API. Figlet windows get this
   for free if they're TWindow subclasses.

3. **Right-click context menu on TFrameAnimationWindow** is being built right now.
   The figlet window should **share the same popup pattern** — extract the menu
   builder into a shared helper rather than duplicating in each window class.

### Simplification opportunities

- **Skip TChromedWindow base for now.** The constructor boilerplate is ~10 lines.
   3 window types isn't enough pain to justify an inheritance refactor. Copy the
   pattern, add a `// TODO: extract TChromedWindow if we hit 4+ types` comment.

- **Workspace save/restore** is missing from the brief. Must add `"figlet_text"`
   to `buildWorkspaceJson()` / `loadWorkspaceFromFile()` with props:
   `{text, font, fg, bg, frameless, shadowless}`. Follow existing pattern.

- **Gallery/arrange compatibility.** Figlet windows should work with
   `/gallery/arrange` — they're just TWindows with measurable bounds. No special
   handling needed if they respond to standard move/resize.

- **Blocking popen risk.** `popen("figlet ...")` is synchronous. For short text
   (<50 chars) it's <10ms, fine. But if someone pipes a paragraph, it blocks the
   TV event loop. Same problem Scramble had before the async popen fix (9dd1fd7).
   For v1 accept the risk with a size guard (truncate to 80 chars). v2 could use
   the same async popen+poll pattern.

- **Existing `api_send_figlet` overlap.** The current `send_figlet` IPC command
   creates a text_editor and injects figlet output. The new `figlet_text` window
   type is better — purpose-built, resizable reflow, font switching. Once
   `figlet_text` lands, deprecate `send_figlet` or have it delegate to the new
   type.

- **Right-click font menu** — rather than hardcoding 20 fonts in C++, consider
   running `figlet -I2` once at startup, caching the list, and building the menu
   dynamically. Keeps the binary font-agnostic. Show top 15 in the popup, put
   "More..." at the bottom opening a dialog.

### Priority call

This is a **visual impact feature** — big ASCII text on a gallery wall looks
great. Should be done *after* the right-click context menu is stable (current
work) so the font picker can reuse that pattern. Estimate: closer to 5h than
7.5h given the landed infra (bg colour, shadow toggle, context menu pattern).
