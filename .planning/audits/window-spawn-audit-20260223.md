# Window/App Spawn Audit and DRY Refactor Spike Plan

Target file: `app/test_pattern_app.cpp`
Date: 2026-02-23

## Scope and counting method

This audit covers all window-opening/spawn paths in `app/test_pattern_app.cpp`, including:

- Menu command handlers (`case cmXxx:`) that directly spawn windows or delegate to helpers
- `api_spawn_*` functions (all overloads)
- Other window creation/open paths (file open helpers, workspace restore, auto-spawn in send APIs, Scramble toggles)

Counting notes:

- **Raw active insert call sites** (`deskTop->insert(...)`, `app.deskTop->insert(...)`, `TProgram::deskTop->insert(...)`): **70** (mechanically counted)
- **Unique `api_spawn_*` names**: **27** (29 definitions including `test`/`gradient` overloads)
- This table set documents **distinct spawn/open code paths** (command/helper/API/workspace) rather than repeating every identical insert call line in a loop-like pattern.

## High-level findings

- The dominant duplication is `bounds calc -> createXxxWindow(...) -> deskTop->insert(...) -> registerWindow(...)`.
- Menu and API paths are **not consistently equivalent** for the same window type (size/placement often differs).
- Several menu spawns and a few API spawns **omit `registerWindow(...)`**, causing registry/event parity drift.
- `api_spawn_*` family already shows a near-uniform pattern and is the lowest-risk first target for a generic helper.

## Spawn Site Inventory

### A. Menu command handlers (direct or delegated)

| Location | Command/function | Default size (w x h) | Centering method | Factory function |
|---|---|---:|---|---|
| `app/test_pattern_app.cpp:945` | `cmNewWindow` -> `newTestWindow()` | `48x14` | Cascade offset (`windowNumber % 10`) | `new TTestPatternWindow(...)` via helper |
| `app/test_pattern_app.cpp:949` | `cmNewGradientH` -> `newGradientWindow(gtHorizontal)` | `48x14` | Cascade offset | `new TGradientWindow(..., gtHorizontal)` via helper |
| `app/test_pattern_app.cpp:953` | `cmNewGradientV` -> `newGradientWindow(gtVertical)` | `48x14` | Cascade offset | `new TGradientWindow(..., gtVertical)` via helper |
| `app/test_pattern_app.cpp:957` | `cmNewGradientR` -> `newGradientWindow(gtRadial)` | `48x14` | Cascade offset | `new TGradientWindow(..., gtRadial)` via helper |
| `app/test_pattern_app.cpp:961` | `cmNewGradientD` -> `newGradientWindow(gtDiagonal)` | `48x14` | Cascade offset | `new TGradientWindow(..., gtDiagonal)` via helper |
| `app/test_pattern_app.cpp:969` | `cmNewDonut` -> `newDonutWindow()` | `48x14` | Cascade offset | `new TFrameAnimationWindow(..., "donut.txt")` via helper |
| `app/test_pattern_app.cpp:973` | `cmOpenAnimation` -> `openAnimationFile()/openAnimationFilePath(...)` | Auto-size from file content | `calculateWindowBounds(path)` | `new TFrameAnimationWindow(...)` via helper |
| `app/test_pattern_app.cpp:982` | `cmOpenTransparentText` -> `openTransparentTextFile()` | `80x24` | Cascade offset | `new TTransparentTextWindow(...)` via helper |
| `app/test_pattern_app.cpp:986` | `cmOpenWorkspace` -> `openWorkspace()` -> `loadWorkspaceFromFile(...)` | From saved workspace bounds | Saved bounds + clamp | `new TTestPatternWindow` / `new TGradientWindow` / `spec->spawn(...)` |
| `app/test_pattern_app.cpp:1029` | `cmTextEditor` | `desktop-10 x desktop-6` | Symmetric inset `grow(-5,-3)` | `createTextEditorWindow(r)` |
| `app/test_pattern_app.cpp:1038` | `cmBrowser` -> `newBrowserWindow()` | `desktop-6 x desktop-4` | Symmetric inset `grow(-3,-2)` | `createBrowserWindow(r)` via helper |
| `app/test_pattern_app.cpp:1050` | `cmAsciiGridDemo` | `desktop-20 x desktop-10` | Symmetric inset `grow(-10,-5)` | `createAsciiGridDemoWindow(r)` |
| `app/test_pattern_app.cpp:1057` | `cmAnimatedBlocks` | `desktop-20 x desktop-10` | Symmetric inset `grow(-10,-5)` | `createAnimatedBlocksWindow(r)` |
| `app/test_pattern_app.cpp:1064` | `cmAnimatedGradient` | `desktop-20 x desktop-10` | Symmetric inset `grow(-10,-5)` | `createAnimatedGradientWindow(r)` |
| `app/test_pattern_app.cpp:1071` | `cmAnimatedScore` | `desktop-24 x desktop-12` | Symmetric inset `grow(-12,-6)` | `createAnimatedScoreWindow(r)` |
| `app/test_pattern_app.cpp:1078` | `cmVerseField` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createGenerativeVerseWindow(r)` |
| `app/test_pattern_app.cpp:1085` | `cmOrbitField` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createGenerativeOrbitWindow(r)` |
| `app/test_pattern_app.cpp:1092` | `cmMyceliumField` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createGenerativeMyceliumWindow(r)` |
| `app/test_pattern_app.cpp:1099` | `cmTorusField` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createGenerativeTorusWindow(r)` |
| `app/test_pattern_app.cpp:1106` | `cmCubeField` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createGenerativeCubeWindow(r)` |
| `app/test_pattern_app.cpp:1113` | `cmMonsterPortal` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createGenerativeMonsterPortalWindow(r)` |
| `app/test_pattern_app.cpp:1120` | `cmMonsterVerse` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createGenerativeMonsterVerseWindow(r)` |
| `app/test_pattern_app.cpp:1127` | `cmMonsterCam` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createGenerativeMonsterCamWindow(r)` |
| `app/test_pattern_app.cpp:1134` | `cmMicropolisAscii` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createMicropolisAsciiWindow(r)` |
| `app/test_pattern_app.cpp:1143` | `cmQuadra` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createQuadraWindow(r)` |
| `app/test_pattern_app.cpp:1152` | `cmSnake` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createSnakeWindow(r)` |
| `app/test_pattern_app.cpp:1161` | `cmRogue` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createRogueWindow(r)` |
| `app/test_pattern_app.cpp:1170` | `cmDeepSignal` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createDeepSignalWindow(r)` |
| `app/test_pattern_app.cpp:1179` | `cmOpenTerminal` | `desktop-4 x desktop-2` | Symmetric inset `grow(-2,-1)` | `createTerminalWindow(r)` |
| `app/test_pattern_app.cpp:1190` | `cmAppLauncher` | `63x20` | Manual center (clamped x/y >= 0) | `createAppLauncherWindow(r)` |
| `app/test_pattern_app.cpp:1204` | `cmAsciiGallery` | `90% desktop x 90% desktop` | Manual center | `createAsciiGalleryWindow(r)` |
| `app/test_pattern_app.cpp:1217` | `cmRogueHackTerminal` | `52x18` | Top-right anchor (`x = desk.w - tw - 1`, `y=1`) | `createTerminalWindow(termBounds)` |
| `app/test_pattern_app.cpp:1268` | `cmDeepSignalTerminal` | `56x20` | Top-right anchor (`x = desk.w - tw - 1`, `y=1`) | `createTerminalWindow(termBounds)` |
| `app/test_pattern_app.cpp:1445` | `cmWibWobChat` -> `newWibWobWindow()` | `80x27` | Cascade offset | `createWibWobWindow(bounds, title)` via helper |
| `app/test_pattern_app.cpp:1527` | `cmNewPaintCanvas` | `min(82,dw) x min(26,dh)` | Manual center | `createPaintWindow(r)` |
| `app/test_pattern_app.cpp:1540` | `cmOpenImageFile` | `68x24` | Cascade offset | `createAsciiImageWindowFromFile(bounds, file)` |

Notes:

- Several direct menu spawns omit `registerWindow(...)` (notably `cmTextEditor`, `cmAsciiGridDemo`, `cmAnimatedBlocks`, `cmAnimatedGradient`, `cmAnimatedScore`, and the 8 generative fields).
- Menu handlers for terminal/app/gallery/paint do register.

### B. Other non-`api_spawn_*` window creation paths (helpers/internal)

| Location | Command/function | Default size (w x h) | Centering method | Factory function |
|---|---|---:|---|---|
| `app/test_pattern_app.cpp:1575` / `:1593` | `newTestWindow()` | `48x14` | Cascade offset | `new TTestPatternWindow(bounds, title)` |
| `app/test_pattern_app.cpp:1596` / `:1605` | `newTestWindow(const TRect&)` | Explicit bounds | Caller-provided | `new TTestPatternWindow(bounds, title)` |
| `app/test_pattern_app.cpp:1609` / `:1614` | `newBrowserWindow()` | `desktop-6 x desktop-4` | Symmetric inset `grow(-3,-2)` | `createBrowserWindow(r)` |
| `app/test_pattern_app.cpp:1620` / `:1623` | `newBrowserWindow(const TRect&)` | Explicit bounds | Caller-provided | `createBrowserWindow(bounds)` |
| `app/test_pattern_app.cpp:1746` / `:1767` | `toggleScramble()` create path | `28x14` | Bottom-right anchor | `createScrambleWindow(r, sdsSmol)` |
| `app/test_pattern_app.cpp:1775` / `:1789` | `toggleScrambleExpand()` create path (when absent) | `30 x full desktop height` | Right edge anchored full-height | `createScrambleWindow(r, sdsTall)` |
| `app/test_pattern_app.cpp:1859` / `:1892` | `newGradientWindow(type)` | `48x14` | Cascade offset | `new TGradientWindow(bounds, title, type)` |
| `app/test_pattern_app.cpp:1896` / `:1920` | `newGradientWindow(type, const TRect&)` | Explicit bounds | Caller-provided | `new TGradientWindow(bounds, title, type)` |
| `app/test_pattern_app.cpp:1946` / `:1964` | `newDonutWindow()` | `48x14` | Cascade offset | `new TFrameAnimationWindow(bounds, "", "donut.txt")` |
| `app/test_pattern_app.cpp:1968` / `:1991` | `newWibWobWindow()` | `80x27` | Cascade offset | `createWibWobWindow(bounds, title)` |
| `app/test_pattern_app.cpp:1996` / `:2017` | `newWibWobTestWindowA()` | `80x27` | Cascade offset | `createWibWobTestWindowA(bounds, title)` |
| `app/test_pattern_app.cpp:2022` / `:2043` | `newWibWobTestWindowB()` | `80x27` | Cascade offset | `createWibWobTestWindowB(bounds, title)` |
| `app/test_pattern_app.cpp:2048` / `:2069` | `newWibWobTestWindowC()` | `80x27` | Cascade offset | `createWibWobTestWindowC(bounds, title)` |
| `app/test_pattern_app.cpp:2074` / `:2089` | `openAnimationFile()` | Auto-size from file content | `calculateWindowBounds(file)` | `new TFrameAnimationWindow(bounds, "", file)` |
| `app/test_pattern_app.cpp:2094` / `:2113` | `openAnimationFilePath(path)` | Auto-size from file content | `calculateWindowBounds(path)` | `new TFrameAnimationWindow(bounds, "", path)` |
| `app/test_pattern_app.cpp:2117` / `:2134` | `openAnimationFilePath(path, bounds)` | Explicit bounds | Caller-provided | `new TFrameAnimationWindow(bounds, "", path)` |
| `app/test_pattern_app.cpp:2138` / `:2168` | `openTransparentTextFile()` | `80x24` | Cascade offset | `new TTransparentTextWindow(bounds, title, file)` |
| `app/test_pattern_app.cpp:2646` / `:2660` | `api_open_text_view_path(path, bounds)` | `80x24` fallback | Explicit or cascade fallback | `new TTransparentTextWindow(r, title, path)` |
| `app/test_pattern_app.cpp:3130` / `:3270` | `loadWorkspaceFromFile(...)` restore insert path | Saved bounds (`x,y,w,h`) | Saved bounds + clamp to desktop | `new TTestPatternWindow`, `new TGradientWindow`, or `WindowTypeSpec::spawn` |
| `app/test_pattern_app.cpp:3524` / `:3561` | `api_send_text(...)` auto-spawn text editor path | `desktop-10 x desktop-6` | Symmetric inset `grow(-5,-3)` | `createTextEditorWindow(r)` |
| `app/test_pattern_app.cpp:3592` / `:3615` | `api_send_figlet(...)` auto-spawn text editor path | `desktop-10 x desktop-6` | Symmetric inset `grow(-5,-3)` | `createTextEditorWindow(r)` |

Notes:

- `loadWorkspaceFromFile(...)` is a major special case because some types are directly constructed (`test_pattern`, `gradient`) while most are delegated through the window-type registry `spec->spawn(...)` and discovered by diffing desktop windows.
- `api_send_text` / `api_send_figlet` use an implicit spawn path outside the `api_spawn_*` family.

### C. `api_spawn_*` functions and menu parity mapping

Common API helper: `api_centered_bounds(...)` at `app/test_pattern_app.cpp:3644` (centered fixed-size clamp to desktop, min `10x6`).

| Location | API function | Default size (w x h) | Centering method | Factory function | Matching menu handler(s) |
|---|---|---:|---|---|---|
| `app/test_pattern_app.cpp:2611` | `api_spawn_test(app)` | `48x14` (via helper) | Cascade (delegates) | `new TTestPatternWindow` via `newTestWindow()` | `cmNewWindow` |
| `app/test_pattern_app.cpp:2624` | `api_spawn_test(app, bounds)` | Explicit or `48x14` | Explicit or cascade | `new TTestPatternWindow` via `newTestWindow(...)` | `cmNewWindow` (no explicit-bounds variant) |
| `app/test_pattern_app.cpp:2612` | `api_spawn_gradient(app, kind)` | `48x14` (via helper) | Cascade (delegates) | `new TGradientWindow` via `newGradientWindow(...)` | `cmNewGradientH/V/R/D` |
| `app/test_pattern_app.cpp:2632` | `api_spawn_gradient(app, kind, bounds)` | Explicit or `48x14` | Explicit or cascade | `new TGradientWindow` via `newGradientWindow(...)` | `cmNewGradientH/V/R/D` |
| `app/test_pattern_app.cpp:3512` | `api_spawn_text_editor` | `desktop-10 x desktop-6` | Symmetric inset `grow(-5,-3)` | `createTextEditorWindow(r, title)` | `cmTextEditor` (same sizing) |
| `app/test_pattern_app.cpp:3635` | `api_spawn_browser` | `desktop-6 x desktop-4` (via helper) | Symmetric inset `grow(-3,-2)` | `createBrowserWindow(...)` via `newBrowserWindow(...)` | `cmBrowser` |
| `app/test_pattern_app.cpp:3655` | `api_spawn_verse` | `96x30` | `api_centered_bounds` | `createGenerativeVerseWindow(r)` | `cmVerseField` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3662` | `api_spawn_mycelium` | `96x30` | `api_centered_bounds` | `createGenerativeMyceliumWindow(r)` | `cmMyceliumField` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3669` | `api_spawn_orbit` | `96x30` | `api_centered_bounds` | `createGenerativeOrbitWindow(r)` | `cmOrbitField` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3676` | `api_spawn_torus` | `90x28` | `api_centered_bounds` | `createGenerativeTorusWindow(r)` | `cmTorusField` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3683` | `api_spawn_cube` | `90x28` | `api_centered_bounds` | `createGenerativeCubeWindow(r)` | `cmCubeField` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3690` | `api_spawn_life` | `90x28` | `api_centered_bounds` | `createGameOfLifeWindow(r)` | No menu match found |
| `app/test_pattern_app.cpp:3697` | `api_spawn_blocks` | `84x24` | `api_centered_bounds` | `createAnimatedBlocksWindow(r)` | `cmAnimatedBlocks` (menu uses `grow(-10,-5)`) |
| `app/test_pattern_app.cpp:3704` | `api_spawn_score` | `108x34` | `api_centered_bounds` | `createAnimatedScoreWindow(r)` | `cmAnimatedScore` (menu uses `grow(-12,-6)`) |
| `app/test_pattern_app.cpp:3711` | `api_spawn_ascii` | `96x30` | `api_centered_bounds` | `createAnimatedAsciiWindow(r)` | No direct menu parity (`cmAsciiGridDemo` uses `createAsciiGridDemoWindow`) |
| `app/test_pattern_app.cpp:3718` | `api_spawn_animated_gradient` | `84x24` | `api_centered_bounds` | `createAnimatedGradientWindow(r)` | `cmAnimatedGradient` (menu uses `grow(-10,-5)`) |
| `app/test_pattern_app.cpp:3725` | `api_spawn_monster_cam` | `96x30` | `api_centered_bounds` | `createGenerativeMonsterCamWindow(r)` | `cmMonsterCam` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3732` | `api_spawn_monster_verse` | `96x30` | `api_centered_bounds` | `createGenerativeMonsterVerseWindow(r)` | `cmMonsterVerse` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3739` | `api_spawn_monster_portal` | `96x30` | `api_centered_bounds` | `createGenerativeMonsterPortalWindow(r)` | `cmMonsterPortal` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3746` | `api_spawn_micropolis_ascii` | `110x34` | `api_centered_bounds` | `createMicropolisAsciiWindow(r)` | `cmMicropolisAscii` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3753` | `api_spawn_quadra` | `42x26` | `api_centered_bounds` | `createQuadraWindow(r)` | `cmQuadra` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3760` | `api_spawn_snake` | `60x30` | `api_centered_bounds` | `createSnakeWindow(r)` | `cmSnake` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3767` | `api_spawn_rogue` | `80x38` | `api_centered_bounds` | `createRogueWindow(r)` | `cmRogue` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3774` | `api_spawn_deep_signal` | `70x34` | `api_centered_bounds` | `createDeepSignalWindow(r)` | `cmDeepSignal` (menu uses near-fullscreen inset) |
| `app/test_pattern_app.cpp:3781` | `api_spawn_app_launcher` | `63x20` | `api_centered_bounds` | `createAppLauncherWindow(r)` | `cmAppLauncher` (manual-center duplicate) |
| `app/test_pattern_app.cpp:3788` | `api_spawn_gallery` | `90% desktop x 90% desktop` | `api_centered_bounds` with percent-derived dims | `createAsciiGalleryWindow(r)` | `cmAsciiGallery` (manual-center duplicate) |
| `app/test_pattern_app.cpp:3853` | `api_spawn_wibwob` | `80x27` | Fixed top-left `TRect(2,1,82,28)` | `createWibWobWindow(r, title)` | `cmWibWobChat` (menu uses cascade) |
| `app/test_pattern_app.cpp:3865` | `api_spawn_terminal` | `80x24` | `api_centered_bounds` | `createTerminalWindow(r)` | `cmOpenTerminal` (menu uses near-fullscreen inset); also special terminal spawns exist |
| `app/test_pattern_app.cpp:3957` | `api_spawn_paint` | `min(82,dw) x min(26,dh)` | Manual center with invalid-size fallback | `createPaintWindow(r)` | `cmNewPaintCanvas` (manual-center duplicate) |

## Duplicate size/centering pattern inventory

### 1. Symmetric desktop inset (`getExtent(); r.grow(-dx,-dy)`) patterns

- `grow(-2,-1)` near-fullscreen: menu `cmVerseField`, `cmOrbitField`, `cmMyceliumField`, `cmTorusField`, `cmCubeField`, `cmMonsterPortal`, `cmMonsterVerse`, `cmMonsterCam`, `cmMicropolisAscii`, `cmQuadra`, `cmSnake`, `cmRogue`, `cmDeepSignal`, `cmOpenTerminal`
- `grow(-10,-5)`: menu `cmAsciiGridDemo`, `cmAnimatedBlocks`, `cmAnimatedGradient`
- `grow(-12,-6)`: menu `cmAnimatedScore`
- `grow(-5,-3)`: menu `cmTextEditor`, `api_spawn_text_editor`, `api_send_text` auto-spawn, `api_send_figlet` auto-spawn
- `grow(-3,-2)`: `newBrowserWindow()` (menu `cmBrowser`, `api_spawn_browser`)

### 2. Cascade pattern (incremental offset using `windowNumber`)

Repeated in:

- `newTestWindow()`
- `newGradientWindow(...)`
- `newDonutWindow()`
- `newWibWobWindow()` and test variants A/B/C
- `openTransparentTextFile()`
- `cmOpenImageFile`
- `api_open_text_view_path(...)` fallback

### 3. Manual centered fixed-size duplicates (menu and API parity pairs)

- App Launcher: menu `cmAppLauncher` manual center vs `api_spawn_app_launcher` using `api_centered_bounds(63,20)`
- Gallery: menu `cmAsciiGallery` manual 90% center vs `api_spawn_gallery` with helper
- Paint: menu `cmNewPaintCanvas` manual center vs `api_spawn_paint` manual center (similar but not shared)

### 4. API centered-fixed presets (`api_centered_bounds`) repeated by many `api_spawn_*`

- `96x30`: verse/mycelium/orbit/ascii/monster_cam/monster_verse/monster_portal
- `90x28`: torus/cube/life
- `84x24`: blocks/animated_gradient
- Other singletons: `108x34` score, `110x34` micropolis, `42x26` quadra, `60x30` snake, `80x38` rogue, `70x34` deep_signal, `80x24` terminal, `63x20` app_launcher

## Mismatches and risks worth fixing during refactor

1. Missing window registration (`registerWindow`) on multiple spawns
- Menu direct spawns omit registration in several places (art/demo/text editor paths).
- `api_spawn_text_editor` also omits registration.
- `newTestWindow()` (no-bounds overload) omits registration, unlike `newTestWindow(const TRect&)`.

2. Menu/API parity mismatches for same logical window type
- Generative/game windows: menu uses near-fullscreen inset; API uses fixed centered sizes.
- Terminal: menu `cmOpenTerminal` near-fullscreen; API terminal `80x24` centered.
- Wib&Wob: menu cascade; API fixed top-left.
- ASCII: `api_spawn_ascii` spawns `createAnimatedAsciiWindow`, while menu `cmAsciiGridDemo` spawns `createAsciiGridDemoWindow` (likely different products under similar naming).

3. Creation side effects mixed into spawn code
- Browser spawn auto-fetches default URL.
- WibWob spawn selects window.
- Scripted terminal spawns send canned text and restore focus.
- Workspace restore uses a separate indirect spawn path (`WindowTypeSpec::spawn`) and desktop diffing.

## Proposed DRY refactor plan

### Target design

Introduce a **single generic spawn helper** for the common path and a small bounds-policy helper.

Suggested shape (C++14-friendly, pointer for optional bounds):

```cpp
struct SpawnOptions {
    bool registerWindow = true;
    bool selectWindow = false;
    bool allowNull = false;
};

enum class BoundsPolicyKind {
    ExplicitOnly,
    CenteredFixed,
    InsetGrow,
    Cascade,
    CenteredPercent,
    AnchorTopRight,
};

struct BoundsPolicy {
    BoundsPolicyKind kind;
    int a = 0; // width / dx / percent / tw
    int b = 0; // height / dy / percent / th
    int xPad = 0; // optional for anchors
    int yPad = 0;
};

TRect resolveSpawnBounds(TTestPatternApp& app, const TRect* requested, const BoundsPolicy& policy);

template <class Factory, class AfterCreate>
TWindow* spawnWindow(TTestPatternApp& app,
                     const TRect* requested,
                     const BoundsPolicy& policy,
                     Factory&& factory,
                     const SpawnOptions& opts,
                     AfterCreate&& afterCreate);
```

Minimal version (matching your suggested shape) is also viable:
- `spawnWindow(factory, defaultW, defaultH, bounds)`
- plus overloads or a `BoundsPolicy` helper for non-centered cases (`grow`, cascade, top-right)

### Unifying menu handlers and API functions

1. Create one shared spawn function per logical window type
- Example: `spawnVerseWindow(app, const TRect* bounds = nullptr, SpawnSource src = SpawnSource::Menu)`
- Internally uses shared `spawnWindow(...)` + a chosen default policy.

2. Make menu and API call the same logical function
- Menu `cmVerseField` and `api_spawn_verse` both call `spawnVerseWindow(...)`.
- If parity should differ intentionally, encode that as an explicit policy parameter (`MenuDefault` vs `ApiDefault`) and document it.

3. Move post-create behaviors into optional callbacks
- Browser `fetchUrl(...)`
- WibWob `select()`
- Scripted terminal `sendText(...)` and focus restoration

4. Centralize registration policy
- Default helper path should call `registerWindow(...)` unless explicitly disabled.
- This will eliminate current accidental omissions.

### Conversion plan by difficulty

#### Trivial conversions (mechanical)

Direct `api_spawn_*` functions in the uniform block (`3655-3793`, plus `3865`, `3957`) are easiest:

- `api_spawn_verse`, `mycelium`, `orbit`, `torus`, `cube`, `life`
- `api_spawn_blocks`, `score`, `animated_gradient`
- `api_spawn_monster_cam`, `monster_verse`, `monster_portal`
- `api_spawn_micropolis_ascii`, `quadra`, `snake`, `rogue`, `deep_signal`
- `api_spawn_app_launcher`, `api_spawn_gallery`, `api_spawn_terminal`, `api_spawn_paint`

Menu equivalents can then become thin wrappers calling the same shared spawn functions.

#### Moderate conversions (side effects but still straightforward)

- `api_spawn_browser` / `cmBrowser` (post-create `fetchUrl`)
- `api_spawn_wibwob` / `cmWibWobChat` (title generation + `select()`)
- `api_spawn_text_editor` / `cmTextEditor` (title override support, registration fix)
- `api_spawn_test` + `api_spawn_gradient` (delegated overloads, registration consistency)

#### Special handling (do not force into same helper on first pass)

- `cmRogueHackTerminal` and `cmDeepSignalTerminal`
  - Need scripted terminal content and focus restoration.
  - Can still reuse a bounds + create/insert/register helper, but keep bespoke post-create flow.
- `toggleScramble()` / `toggleScrambleExpand()`
  - Not pure spawn commands; they are toggle/lifecycle/state transitions.
- `openAnimationFile*`, `openTransparentTextFile`, `cmOpenImageFile`
  - File dialogs and content-dependent bounds make them separate but still reusable for insert/register helper.
- `loadWorkspaceFromFile(...)`
  - Restore semantics + `WindowTypeSpec::spawn` desktop-diff logic make this a later phase.

## Estimated effort

- **Medium** for a first refactor slice that unifies `api_spawn_*` + matching menu handlers and fixes registration inconsistencies.
- **Large** if the same pass also normalizes file-open helpers, scripted terminal spawns, Scramble toggles, and workspace restore.

Recommended slice for first PR: **Medium** (API/menu parity + generic helper + registration fixes).

## Draft Spike Brief (`.planning` format)

```md
# spkNN-window-spawn-dry-refactor-brief

Status: not-started
GitHub issue: #NNN
PR: —

## Summary
Spike the window spawn/open pathways in `app/test_pattern_app.cpp` and prototype a DRY helper that centralizes bounds resolution, window creation, desktop insertion, and registration while preserving menu/API behavior.

## Goal
Produce an implementation-ready refactor plan (and optionally a small proof-of-concept patch) that removes duplicated spawn boilerplate across menu handlers and `api_spawn_*` functions without changing visible behavior unless explicitly documented.

## Acceptance Criteria

- AC-1: Inventory every window spawn/open path in `app/test_pattern_app.cpp` (menu, `api_spawn_*`, helper/internal restore/auto-spawn paths) with default bounds behavior and factory function.
  Test: Run `rg -n -C3 "deskTop->insert\(" app/test_pattern_app.cpp` and verify every active insert site is classified in the spike notes/table.

- AC-2: Identify all menu/API parity pairs and document current mismatches in default size/placement/registration behavior.
  Test: Run `rg -n "^void api_spawn_" app/test_pattern_app.cpp` and cross-check each function against `case cm` handlers in `handleEvent(...)`.

- AC-3: Define a generic spawn helper API (bounds resolution + create/insert/register) that is implementable in C++14 and supports explicit bounds overrides.
  Test: Build a compile-targeted prototype signature in a scratch patch or design snippet and confirm it can express at least one centered-fixed and one inset-grow spawn.

- AC-4: Identify a safe first refactor slice and list exact call sites for conversion in PR-1.
  Test: Produce a conversion checklist referencing concrete line locations in `app/test_pattern_app.cpp` and verify no special-case paths (workspace restore/scripted terminals) are included unless explicitly scoped.

- AC-5: Preserve AC/Test traceability for the follow-on implementation story/PR.
  Test: Draft the follow-on story brief with each `AC-*` immediately followed by a concrete `Test:` line and validate no placeholders (`TBD`/`TODO`) remain.

## Non-Goals

- Rewriting workspace restore (`loadWorkspaceFromFile`) in the same PR unless required for helper extraction.
- Changing default window sizes/placement for menu/API parity without an explicit product decision.
- Refactoring Scramble toggle lifecycle behavior beyond optional helper reuse for insertion/registration.

## Risks / Notes

- Registry behavior may change if missing `registerWindow(...)` calls are normalized; downstream API/state tests should be run.
- Some menu/API size mismatches may be intentional UX choices and need confirmation before unifying defaults.

## Evidence

- `rg -n -C3 "deskTop->insert\(" app/test_pattern_app.cpp`
- `rg -n "^void api_spawn_" app/test_pattern_app.cpp`
- `rg -n "case cm" app/test_pattern_app.cpp`

## Rollback

Refactor should be structured so each spawn-site conversion is a small commit; rollback can revert individual converted windows while leaving the helper in place.
```
