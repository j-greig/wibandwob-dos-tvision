# skins/

Skin data files for wwdos's CGA-styled Turbo Vision chrome. Each `.skin` file
is a plain line-based key/value format — one key per line, `#` starts a
comment, unknown keys are ignored.

## Format

```
# <name>.skin — one-line vibe description
name <lowercase-identifier>
texture <single UTF-8 glyph or omit for solid>   # desktop fill: ▒ ░ · ∴ ~ etc
desk <fg> <bg>          # desktop dither colours, CGA indices 0-15
paper <bg> <fg>         # default viewer window body (NOTE: bg first)
dialog <bg> <fg>        # accent window body (bg first)
framePassive <attr>     # BIOS byte bg*16+fg, decimal or 0xNN; omit = derive
frameActive <attr>
menu <attr>             # menu/status bar colours; omit = classic black-on-white
dim <fg>                # hint text; omit = grey 8
accent <fg>             # highlight ink; omit = dialogFg
floor <fg> <bg>         # chromeless room interior; omit = desk
ok <fg>                 # healthy indicator; omit = 10
warn <fg>               # warning; omit = 12
pal0 #RRGGBB            # optional: remap terminal ANSI slot 0..15 —
...                     # THE monitor swap. omit any slot = authentic CGA
pal15 #RRGGBB           # (CGA: 0 #000000 1 #0000AA 2 #00AA00 3 #00AAAA
                        #  4 #AA0000 5 #AA00AA 6 #AA5500 7 #AAAAAA
                        #  8 #555555 9 #5555FF 10 #55FF55 11 #55FFFF
                        #  12 #FF5555 13 #FF55FF 14 #FFFF55 15 #FFFFFF)
```

Notes:

- `paper` and `dialog` take **bg first, then fg** — easy to get backwards,
  double-check when authoring a new skin.
- `framePassive` / `frameActive` / `menu` are classic BIOS text-mode attribute
  bytes: `bg * 16 + fg`, either decimal or `0xNN` hex. Leave any of them out
  and the app derives a value from `paper`/`dialog` — fine for light skins,
  but **dark skins must set all three explicitly** or they inherit the
  default daylight slate chrome, which looks wrong against a dark body.
- `pal0..pal15` remap what each CGA index actually *renders as* on the
  terminal (the ANSI slot swap). This is the mechanism that turns "index 4 =
  red" into "index 4 = blood red #5C0A0A" — the skin can keep using CGA
  indices semantically (0 black, 4 red, 12 light red, etc.) while completely
  retinting the monitor. Omit any slot to leave it as authentic CGA.

## CGA index cheat-sheet

| idx | name         | authentic hex |
|-----|--------------|---------------|
| 0   | black        | `#000000`     |
| 1   | blue         | `#0000AA`     |
| 2   | green        | `#00AA00`     |
| 3   | cyan         | `#00AAAA`     |
| 4   | red          | `#AA0000`     |
| 5   | magenta      | `#AA00AA`     |
| 6   | brown        | `#AA5500`     |
| 7   | light grey   | `#AAAAAA`     |
| 8   | dark grey    | `#555555`     |
| 9   | light blue   | `#5555FF`     |
| 10  | light green  | `#55FF55`     |
| 11  | light cyan   | `#55FFFF`     |
| 12  | light red    | `#FF5555`     |
| 13  | light magenta| `#FF55FF`     |
| 14  | yellow       | `#FFFF55`     |
| 15  | white        | `#FFFFFF`     |

## Applying a skin

- `set_skin <name>` picks up new `.skin` files in this directory
  automatically — no restart or manual registration needed, just add the
  file and reference it by its `name` key.
- `reload_skins` re-reads all `.skin` files from disk, picking up edits to
  ones already loaded.
- `skin_save` writes the currently-active skin back out to its `.skin` file
  (useful after live-tweaking colours in-app and wanting to persist them).

## The six shipped skins

| file | vibe |
|------|------|
| `vaporwave.skin` | Sunset neon grid — magenta/cyan on deep purple-black, full palette remapped to soft neon pastels. |
| `gameboy.skin`   | 4-shade olive-green DMG LCD — all 16 CGA slots collapse onto the four classic Game Boy greens. |
| `amber-crt.skin` | Warm single-tube amber terminal — one-hue ramp from near-black to bright amber, sparse scanline dither. |
| `bloodmoon.skin` | Near-black ritual reds with bone-white cutting through — sparse `∴` dither, deepest of the six. |
| `seafoam.skin`   | Pale aquatic daylight — light skin, no chrome overrides, authentic CGA cyan/brown do the work unaided. |
| `c64.skin`       | Commodore 64 tribute — classic light-blue-on-blue boot screen, authentic VIC-II 16-colour palette. |

## Paper variants (canon rule: several window identities per scheme)

```
paper2 <bg> <fg>    # extra window identities; set_skin distributes
paper3 <bg> <fg>    # round-robin across colourable windows
paper4 <bg> <fg>
frames chunky       # opt into the fat Figma-style block frames
```

## Polychrome interiors

Primer/text files may contain ANSI SGR sequences (`ESC[34m` etc) — the
viewers render them as coloured runs (SGR→CGA, bright via 90s/bold).
Plain files pay nothing. See modules/wibwob-primers/primers/
standort-card.txt for the grammar: labels one colour, values another,
alarms red.
