# kernel/

The parts of WibWob-DOS that do not depend on the app: the layers that put Turbo Vision on a
screen and take input from a person. The app (`app/`) builds on top of them.

What belongs here:

- the Turbo Vision web platform (`platform/web/`): the browser console as patches to
  `vendor/tvision`, and the CMake code that applies them to a copy of the submodule
- the cell renderer and the page shells (`pages/`): the xterm.js page and the WebGL2 page
  that draws the cell grid itself from a glyph atlas
- the tools that make pages (`tools/`): `bundle.py` (one self-contained HTML file per
  page) and `make_atlas.py` (the glyph atlas)
- the browser build (`CMakeLists.txt`, `build-web.sh`)
- the film page (`pages/shell-film.html`, `pages/atlas-film.js`): the page the stage driver's `film` target is bundled into;
  its build is committed as `tfilm/base/film.html` (`tfilm/README.md`)

Not here: the stage driver and its command table (`app/stage/`) and the film's desktop (`app/film/`). The driver
includes app views (the frame player, the animation window, the EGA theme), so it lives in `app/`; see
`docs/stage-driver.md`.

Rule: nothing in `kernel/` includes from `app/`. Code here may know Turbo Vision and the
browser, never a WibWob-DOS view or command. The one link the other way is the build:
`kernel/CMakeLists.txt` adds `app/web/` when `WWDOS_WEB_APP` is on, and `app/web/`
holds the app's browser target and its stand-ins (no PTY, no Micropolis engine); it adds
`app/film/` (the film player) when `WWDOS_WEB_FILM` is on.

Build and test: `docs/web-port.md`.

```
kernel/
├── CMakeLists.txt          browser build (emcmake); hello demo + app/web (WWDOS_WEB_APP) + app/film (WWDOS_WEB_FILM)
├── build-web.sh            configure, build, bundle into dist-web/*.html (film.html with shell-film.html)
├── platform/web/
│   ├── tvision.cmake       copies vendor/tvision, applies patches/, builds libtvision
│   └── patches/tvision/    0001 TermIO CSI bound fix, 0002 browser console (wasmcon), 0003 TVISION_MOUSE_CURSOR
├── pages/
│   ├── shell.html          xterm.js page (ANSI output)
│   ├── shell-webgl.html    WebGL2 cell renderer page (no xterm.js)
│   ├── shell-film.html     the film page: WebGL2 cells + stage clock, sound, gate, scrubber, live API
│   ├── atlas.js            default glyph atlas (GNU Unifont subset)
│   ├── atlas-film.js       the film page's default atlas (make_atlas.py --film)
│   ├── film-host.js        many films on one page sharing one engine download (the film tools' split delivery)
│   ├── sfx-default.json    the house set of synthesised window sounds the film tools put into a page
│   └── vendor/             xterm.js 5.5.0 + fit and webgl addons, with their licence
└── tools/
    ├── bundle.py           page template + Emscripten SINGLE_FILE js -> one html file (--shell, --atlas)
    └── make_atlas.py       Unifont .otf -> pages/atlas.js; --film -> pages/atlas-film.js
```
