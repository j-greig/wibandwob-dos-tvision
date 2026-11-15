# Web port

WibWob-DOS runs in a browser as one self-contained HTML file: the Turbo Vision C++ app compiled
to WebAssembly with Emscripten, inlined as base64 into a page that draws the screen and sends
keys and mouse events. The page makes no network requests and opens from `file://`.

Two page shells share one wasm build:

| page | draws with | output |
|---|---|---|
| `kernel/pages/shell.html` | xterm.js 5.5.0 (WebGL addon, DOM fallback) | ANSI from Turbo Vision's own `AnsiScreenWriter` |
| `kernel/pages/shell-webgl.html` | its own WebGL2 cell renderer, glyphs from `kernel/pages/atlas.js` | a uint32 cell grid in wasm memory |

The app picks the display at start-up: `WasmGridDisplay` when the page defines
`Module.wwdosGrid`, otherwise the ANSI `WasmDisplay`.

```
Turbo Vision ─▶ DisplayBuffer ─┬─ WasmDisplay ──ANSI──▶ Module.wwdosWrite ─▶ xterm.js
                               └─ WasmGridDisplay ──cells──▶ Module.wwdosGrid ─▶ WebGL2 + atlas
page keys / mouse / paste ─▶ byte queue (Module.wwdosQueue) ─▶ WasmInput ─▶ TermIO parser
event wait ─▶ wasmWaitForInput(ms): EM_ASYNC_JS promise, resolves on input or timeout (ASYNCIFY)
```

## Build

Needs emsdk. Homebrew's `emscripten` package stalled during install on macOS; install emsdk
instead (https://emscripten.org/docs/getting_started/downloads.html), for example in `~/emsdk`:

```
git clone https://github.com/emscripten-core/emsdk ~/emsdk && cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
git submodule update --init vendor/tvision
kernel/build-web.sh            # --clean wipes build-web/ first
open dist-web/wwdos.html       # or wwdos-webgl.html, hello.html, hello-webgl.html
```

`build-web.sh` sources `$EMSDK/emsdk_env.sh` (default `~/emsdk`) when `emcc` is not on the path,
configures `kernel/` with `emcmake` into `build-web/`, builds, and runs `kernel/tools/bundle.py`
to write `dist-web/<target>.html` (xterm.js page) and `dist-web/<target>-webgl.html` (WebGL page).

Targets:

| target | what | option |
|---|---|---|
| `hello` | Turbo Vision's own hello demo, the smallest check of the platform layer | always |
| `wwdos` | the WibWob-DOS desktop (`app/web/CMakeLists.txt`) | `WWDOS_WEB_APP`, on by default |
| `film` | the terminal-film player: the stage driver (`app/stage/`) on a small desktop (`app/film/`), bundled with `kernel/pages/shell-film.html` into `dist-web/film.html` only (no xterm.js page). The same wasm as the on-chain repository's `wwdos-min`; see `docs/stage-driver.md` | `WWDOS_WEB_FILM`, on by default |

The desktop (`wwdos`) cannot play a stage; only `film` links the driver. The film's native terminal build is
`cmake -B build-stage -S app/film && cmake --build build-stage -j8` (`build-stage/wwdos-stage`, ncursesw).

The native build (top-level `CMakeLists.txt`) does not read `kernel/` and is unchanged.

### Turbo Vision patches

`vendor/tvision` is a git submodule. The browser build copies it into `build-web/tvision-src`
and applies `kernel/platform/web/patches/tvision/*.patch` in order; the submodule checkout is
not modified. The patches:

| patch | change | guarded |
|---|---|---|
| 0001 | `CSIData::readFrom` wrote one past `_separators` on a CSI sequence with too many parameters | no: a fix for every platform |
| 0002 | browser console: `wasmcon.cpp`/`wasmcon.h` (new), `conctl.cpp`, `events.cpp`, `platform.cpp`, `strings.cpp`, `util.h` | `__EMSCRIPTEN__` |
| 0003 | `TVISION_MOUSE_CURSOR=1` draws the mouse as a reversed cell and asks for plain motion reports (the film's native build); off when unset | no: the variable is never set in a browser |

### Glyph atlas

`kernel/pages/atlas.js` is the default atlas: CP437 plus every non-ASCII character in the
primer library and the app's sources (423 characters, 454 glyph cells, 14.3 KB, 4.8 KB gzip).
A character outside the atlas draws as `?`. To rebuild it after adding primers or menu text,
get GNU Unifont (https://unifoundry.com/unifont/, the `.otf` in `font/precompiled/` of the
release archive; the font is not in this repository) and run:

```
uv run kernel/tools/make_atlas.py --font /path/to/unifont-16.0.04.otf [--extra "chars"] [--png sheet.png]
```

## Test

```
uv run tests/web/smoke.py                   # every dist-web/*.html
uv run tests/web/smoke.py dist-web/wwdos-webgl.html
python3 tests/stage/test_commands_def.py    # the stage driver's command lists against app/stage/commands.def
```

Headless Chrome through Playwright (the installed Google Chrome; `SMOKE_CHANNEL=` uses
Playwright's own Chromium). For each page: load from `file://` with every other request blocked
and counted, wait for the menu bar, open the first menu by its hot key, then open a window
(the demo's Greeting dialog; in the desktop, check the four start-screen windows and open a
Test Pattern window with Ctrl+N; in `film.html`, check the boot scene and tile it from the Window
menu). Fails on any network request or page error.

## What works

Checked with headless tests in the build this port comes from; the smoke test on this branch
re-checks the menu bar, a menu, a dialog, the start screen and opening a window.

- Menus by F10, Alt+letter and mouse; dialogs and other nested modal loops (ASYNCIFY).
- Window raise, drag, resize, tile and cascade; page resize reflows the desktop.
- Animated and generative views at the rate their timers ask for.
- The text editor, the primer browser (Ctrl+O) over the embedded primer library
  (`modules/wibwob-2026/primers`, 112 files, 452 KB), paste into dialogs (bracketed paste).
- On start the desktop opens four primers from the library as its start screen.

## Cut in the browser build

| part | state | why |
|---|---|---|
| Terminal window (tvterm, libvterm) | `app/web/tvterm_stub.cpp`: a window that says there is no terminal | needs a PTY and a shell |
| IPC API server (`api_ipc.cpp`) | compiled, never started; HMAC returns zeros | Unix sockets, CommonCrypto/OpenSSL |
| Micropolis | `app/web/micropolis_stub.cpp`: the window opens with an empty city | the engine is not built for the browser |
| Backrooms TV | starting a stream returns false | `forkpty` and a child process |
| Contour map, Generative Lab helpers | the helper path lookup returns nothing | Python child processes |
| LLM chat, Room Chat, Scramble replies | compiled; no CLI and no key are found, so LLM is off | the `claude` CLI or an API key and network |
| Browser window | fetches fail | network and a helper process |
| Monster cam view | compiled; no face data arrives | reads from a Python/OpenCV worker process |
| TR-808 sound | compiled; no audio plays | plays samples through `afplay` child processes |
| FIGlet text | no output | calls the host's `figlet` program |
| Clipboard | not wired | browsers allow clipboard reads only on a user gesture |
| Workspaces | saved to the in-memory file system, lost on reload | no persistent storage wired |

Start-up in the browser opens the start screen and returns from the app constructor before the
IPC server, layout restore and Scramble engine.

## Sizes

Measured with emcc 6.0.13 (emsdk), `-Os`, `SINGLE_FILE`, on this branch:

| page | raw | gzip -9 | brotli -q 11 |
|---|---|---|---|
| `dist-web/wwdos.html` (xterm.js) | 3,370,222 | 903,531 | 651,127 |
| `dist-web/wwdos-webgl.html` | 2,997,271 | 818,008 | 586,399 |
| `dist-web/hello.html` (xterm.js) | 923,857 | 257,240 | 202,755 |
| `dist-web/hello-webgl.html` | 550,906 | 172,220 | 138,068 |
| `dist-web/film.html` (film page, measured 2026-11-15 in a Linux container) | 1,057,192 | 302,516 | |

The WebGL page is about 85 KB gzip smaller than the xterm.js page: xterm.js with its addons and
CSS is replaced by the atlas and the renderer. The primer library is about 450 KB of the
desktop's raw size. Clean build of both targets: 52 s wall clock on an M-series Mac (10 jobs).
Boot to menu bar in headless Chrome: 0.1-0.5 s.

## Limits and known issues

- ASYNCIFY makes the wasm larger and slower than a build without it. Removing it would mean
  rewriting every `execView` caller as main-loop ticks.
- The xterm.js page sends F1-F4 as `ESC O P`..`ESC O S`, which Turbo Vision's TermIO reads as
  Alt+O plus a letter. The WebGL page sends `ESC[11~`..`ESC[14~` instead and is not affected.
- A mouse drag that starts on the title of an unfocused window raises the window without
  moving it.
- The WebGL renderer draws reverse video and underline; other text styles are ignored.
- Kaomoji characters outside the system font fall back to whatever glyphs the browser finds on
  the xterm.js page. The WebGL page draws only atlas glyphs.
- Tested in headless Chrome only.
