# Stage driver

The terminal-film engine plays a stage file: a timed script of window commands (`@T open NAME X Y W H /boot/F.txt|TITLE`,
`move`, `say`, `shake`, ...) on a small Turbo Vision desktop, one self-contained HTML page per film. It was written in
the on-chain repository (`j-greig/wibandwob-dos-onchain`, `wasm/minimal/`). Its sources now live here and the
on-chain repository builds from a pinned copy of them.

The desktop app (`wwdos`) does not link the driver and cannot play a stage. The two share source files, not a feature.

## What lives where

| here | what | on-chain copy |
|---|---|---|
| `app/stage/stage.h`, `stage.cpp` | the driver: parses a stage file, runs it on a clock (seek, loop, pause, live commands), the grammar as a comment in `stage.h` | `wasm/minimal/stage.h`, `stage.cpp` |
| `app/stage/mirror_view.*`, `picture.*` | a window that shows the live desktop; the PAINT and picture-film reader | `wasm/minimal/` |
| `app/stage/commands.def` | the command table (below) | `wasm/minimal/commands.def` |
| `app/film/minimal_app.cpp` | the film's desktop: menu bar, status line, scrubber, sound and trace hooks for the page, native stand-ins; implements `StageHost` | `wasm/minimal/minimal_app.cpp` |
| `app/film/boot/` | the five primers embedded as `/boot` (the boot scene) | `vendor/boot/` (compared, not copied: see below) |
| `app/film/CMakeLists.txt` | the `film` browser target and the native `wwdos-stage` target | `CMakeLists.txt` (`wwdos-min`, `WWDOS_NATIVE`) |
| `app/frame_file_player_view.*`, `windows/frame_animation_window.*`, `gradient.*`, `ega_theme.*`, `wibwob_background.*`, `notitle_frame.h` | the app views the film compiles | `vendor/wwdos-app/` |
| `kernel/pages/shell-film.html`, `atlas-film.js`, `film-host.js`, `sfx-default.json` | the film page (WebGL cells plus the stage clock, sound, gate, scrubber, live API), its default atlas, the split-delivery host, the house window sounds | `web/shell-webgl.html`, `web/atlas.js`, `web/film-host.js`, `web/sfx-default.json` (copies, not synced) |
| `tfilm/base/film.html` | the built film page, byte-identical to the on-chain `dist/wwdos-min-webgl.html`; `tfilm/scripts/check-base.sh` compares it with a fresh build | `dist/wwdos-min-webgl.html` |

`StageHost` (in `stage.h`) is the boundary: the driver calls the host to open windows, draw marks, play sounds and
read the desktop; the host owns the application. The driver includes three app files (the frame player, the
animation window, the EGA theme), so it sits in `app/`, not `kernel/` (`kernel/` includes nothing from `app/`).

## Build

```
kernel/build-web.sh                               # also builds build-web/film.js and dist-web/film.html
cmake -B build-stage -S app/film && cmake --build build-stage -j8     # build-stage/wwdos-stage (ncursesw)
```

`film` is on by default (`WWDOS_WEB_FILM`). Its sources, their order, the defines (`WWDOS_WASM=1 WWDOS_MINIMAL=1`), the
link flags and the exported runtime methods (`FS,ccall,UTF8ToString`) copy the on-chain `wwdos-min` target, so the two
builds produce the same bytes. The native target reuses the patched Turbo Vision copy from
`kernel/platform/web/tvision.cmake` without `wasmcon.cpp`, with `HAVE_NCURSES`, as the on-chain `WWDOS_NATIVE` build does.
`dist-web/film.html` with no stage opens the boot scene; the on-chain tools (`tools/scene.py`, `tools/stage.py`) turn the
same base page into a film page by writing `/boot/stage.txt` and its primers in `preRun`.

Sizes (emcc 6.0.13, `-Os`, `SINGLE_FILE`): `build-web/film.js` 1,009,479 bytes; `dist-web/film.html` 1,057,192 bytes raw,
302,516 bytes gzip -9.

## Proof that it is the same engine

Built in one container from the on-chain branch `tfilm-phases` at 07b563a3 and this branch:

- `build-web/film.js` here and the on-chain `build/wwdos-min.js` are the same file (sha256 `ebd4d39f6b7d26dc...`; at
  44bf1776, before the on-chain gotchas merge, both were `bd28fefa8f3be7f0...`). The page bundled from it equals the
  on-chain `dist/wwdos-min-webgl.html`.
- `build-stage/wwdos-stage` here and the on-chain `build-native/wwdos-stage` are the same file (sha256 `f8cab435679c8f2c...`).
- On that base page, in the on-chain repository: `tools/stage.py timecheck` and `tools/stage_time_check.py native` (with
  this branch's `wwdos-stage`) report 0 failures; `pytest tests/stage` passes (174 tests).
- `scripts/rebuild-pages.sh --pages-only` on that base page rebuilt 47 film pages (3 have no stage source on the branch)
  and every one is byte-identical to the page committed at 07b563a3. `tools/stage_regress.py compare` on 13 films
  shows the same changes against `tests/frames/` with either base: 8 films unchanged, and hello-tall, hyperpop,
  stackdive, storm and tt-pie changed in the last column of text windows and in titles. Those frames were stored at
  58ba9649, before the gotchas merge (whole-width text, titles to width minus 4); its frame report is still to come.

## Drift found when the driver moved

The on-chain `vendor/wwdos-app/` was copied from `app/` at 3517dfcc; this repository was at 4cadbc1. Every file of the
copy against both:

| file | on-chain edit | core change since 3517dfcc | what was done |
|---|---|---|---|
| `frame_file_player_view.cpp` | first frame kept; `ww_anim_clock`, `ww_anim_paused`, `ww_anim_step`; draw by cells | draw by cells (the same fix, other wording) | on-chain edits applied here, core wording of the draw fix kept |
| `frame_file_player_view.h` | `clockStart_`, `clockIndex()`, `stepSeen_`; later `useWholeWidth()` | none | applied |
| `wibwob_background.h` | `ega_solid` preset, `baseAttr()` | none | applied |
| `api_ipc.cpp`, `ascii_grid_view.cpp`, `backrooms_tv_view.cpp`, `contour_map_view.cpp`, `generative_{cube,lab,monster_portal,mycelium,orbit,torus}_view.cpp`, `tvterm_view.{h,cpp}` | browser guards, growMode fix | the same edits (the web port brought them here) | nothing: identical |
| `wwdos_app.cpp` | boot scene reads `/boot/*.txt` | boot scene reads the embedded primer library; TR-808 | nothing: the on-chain desktop page is not a film file; its copy stays at 3517dfcc |
| `command_registry.cpp`, `figlet_text_view.cpp`, `window_type_registry.cpp` | none | TR-808, clip FIGlet lines by cells | nothing: not used by the film; on-chain copy lacks them |
| `tr808_*`, `web/*_stub.cpp` | not in the copy | new here | nothing |

Every other file of the copy equals 3517dfcc and 4cadbc1.

Turbo Vision: the on-chain `vendor/tvision/` against the submodule with patches 0001 and 0002 applied differs in
comment wording (`termio.cpp`, `platform.cpp`, `wasmcon.{h,cpp}`), in line endings (`util.h`, CRLF here) and in one
change: `TVISION_MOUSE_CURSOR=1` draws the mouse as a reversed cell (`internal/platform.h`, `platform.cpp`, `termio.cpp`),
unguarded. It compiles into the browser build too, so it is now patch 0003; with the variable unset nothing changes.

The desktop changes these edits bring: a drawing's first frame (the text before the first `----`) now plays, so a
two-frame drawing moves where it stood still; `ega_solid` appears in the desktop's preset menu. With no stage clock set
(`ww_anim_clock` null, the desktop's case) the frame player counts its timer events as before.

## How the two repositories stay in step

- Edit the engine here. In the on-chain repository, `python3 tools/vendor_sync.py PATH-TO-THIS-CHECKOUT [--rev REV]`
  copies the files above into their old paths, heads each `wasm/minimal/` file with a one-line note naming its source,
  and writes the commit and a hash of each file to `vendor/core-pin.json` and `vendor/SOURCES.md`.
- `tools/vendor_sync.py --check [--core PATH]` fails when a copy differs from the pin (the on-chain test suite and its
  pre-commit hook run it).
- An engine change made on the on-chain side goes up with `tools/vendor_sync.py --to-core PATH`: a three-way merge
  (base: the file here at the pin) into this checkout's working tree, for a pull request; then a sync brings it back.
- `vendor/boot/` is embedded in the deployed on-chain token, so the sync never writes it; it reports when it differs
  from `app/film/boot/`. The on-chain token build is frozen: films follow the newest engine, the token does not.

## Command table

`app/stage/commands.def` has one line per stage command: kind (timed line or header), the fewest words, whether the
first word names a window, whether it takes `|text`, whether it runs live, and what it does. Checked against it:

- here, `tests/stage/test_commands_def.py` (plain Python): `stage.cpp`'s `known[]` and `notLive[]` (checkLive) and both
  `needWin[]` lists;
- on-chain, `tests/stage/test_command_table.py`: the compiler's `COMMANDS` in `tools/stagelib.py`, the same engine lists
  in the copy, and the command table in `docs/stage.md`.

Nothing is generated from it yet: the dispatch in `Stage::run` and `COMMANDS` are still written by hand.

## Not done

- The dispatch is not generated from `commands.def`; the minimum-word counts are checked against the compiler only (the
  engine checks the count of `open` alone).
- The desktop app cannot play a stage (by decision).
- The on-chain desktop page (`wwdos`) and the rest of its `vendor/wwdos-app/` stay on the 3517dfcc copy; only the
  film's files are synced.
- The film page files are copied here, not synced: `kernel/pages/shell-film.html`, `film-host.js` and
  `sfx-default.json` are byte copies of the on-chain `web/shell-webgl.html`, `web/film-host.js` and
  `web/sfx-default.json` (`shell-film.html` at the on-chain `film-input` branch, with the x-ray view; the other two
  at c7e5d29). A change to the on-chain copies must be brought here by hand until the film tools move.
- X-ray (on-chain `docs/stage.md` § X-ray): the driver's part is read-only. `Stage::objectsJson()` (with
  `StageHost::stageMarkRect`) is the map of windows and marks the page boxes; the live line `xray MODE` is handed to the
  host (`stageXray`) before anything runs or is traced. A script line `xray` warns and does nothing.
- The open log (stage.h, Log): `Stage::runLive` keeps each live line with its tick and script position; `replay` puts
  logged lines back among the script's; with recording on (`record(true)`) the log outlives seeks and loops. The
  host exports `wwdos_stage_log`, `wwdos_stage_record`, `wwdos_stage_log_clear`, `wwdos_stage_log_load` (applied at idle)
  and `wwdos_stage_clock`.
- The core's native desktop build (`wwdos`) was not built here (it needs the macOS-era dependencies); the app edits
  were built in the browser desktop (`dist-web/wwdos*.html`, smoke test passes) and in the film targets.
