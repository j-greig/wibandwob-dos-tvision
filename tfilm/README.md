# tfilm/

The terminal-film tools' home in the core. It holds one file so far.

`base/film.html` is the built film engine page: the `film` target (the stage driver in `app/stage/` on the film
desktop in `app/film/`) bundled with `kernel/pages/shell-film.html` and `kernel/pages/atlas-film.js`. Every film
page is built on top of it, so a film session needs no emsdk. It is byte-identical to `dist/wwdos-min-webgl.html`
of the on-chain repository (j-greig/wibandwob-dos-onchain) at c7e5d29, sha256
`75f65061863d68e4de39c26e4abbc791cb150f6143d9c2934f2b96766d59b410`.

Rebuild and check:

```
kernel/build-web.sh                  # writes dist-web/film.html (emsdk at $EMSDK or ~/emsdk; JOBS=4 limits the jobs)
tfilm/scripts/check-base.sh          # builds, then exits 1 if dist-web/film.html differs from base/film.html
tfilm/scripts/check-base.sh --no-build
```

Run the check after any change under `app/stage/`, `app/film/`, the app views the film compiles (listed in
`app/film/CMakeLists.txt`), `kernel/pages/`, `kernel/tools/bundle.py`, `kernel/platform/web/` or
`vendor/tvision`. An intended engine change copies `dist-web/film.html` here in the same commit.

The film atlas comes from `uv run kernel/tools/make_atlas.py --film`; its glyph set includes the film sources
(`stages/`, `scenes/`), which are not in this repository yet, so until they are, give the on-chain checkout:
`--film-root ~/Repos/wibandwob-dos-onchain`.
