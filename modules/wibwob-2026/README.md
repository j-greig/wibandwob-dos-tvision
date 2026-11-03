# wibwob-2026 primer shelf

Current Wib&Wob drawings for WIBWOB-DOS: the brand kit, the beings, places, the GLITCH talk's
own plates, textures, props and big set pieces. Rebuild with `uv run modules/wibwob-2026/build.py`.

## How to open a piece

- Use the full `path` that `GET /primers/list` returns, e.g.
  `modules/wibwob-2026/primers/being-niff-small.txt`. A bare filename resolves to the first
  module folder the app finds, which may be `example-primers`.
- `POST /menu/command` with `{"command":"open_primer","args":{"path":P,"x":X,"y":Y,"w":W,"h":H,"title":T}}`.
  Window size = piece size + 2 each way (the sizes below already include one spare column).
- Pieces ending `-anim` play their frames at about 3 frames a second. Their windows are wider
  than the drawing (blank on the right): the animation view cuts lines by bytes, not characters.
- Faces render correctly: Wib and Wob kaomoji, Scramble `/ᐠ｡ﻌ｡ᐟ\`, Niff `₍ᐢ..ᐢ₎`. Do not type
  `~` into a primer (the window eats it) or Scramble's old mouth U+A788 (draws as a box).
  Bartholomew's eye `۝` (U+06DD) draws as a box; his face is shown as `つ◎‿◎‿◎༽つ` here.
- This module is first choice; `modules/example-primers/` holds the old demo pieces.

## The shelf (name, width x height in characters)


### THE CAST: canon Wib and Wob, Scramble, the beings and friends (start here)

- `face-wib-and-wob-tall` 26x15: the canon pair, Wib left, Wob right
- `face-wib-and-wob-tripods` 25x9
- `face-wib-and-wob-small-pair` 15x9
- `figure-wib-standing-large` 12x14: large canon Wib
- `figure-wob-standing-large` 12x14: large canon Wob
- `figure-wib-tripod` 12x9
- `figure-wob-tripod` 12x9
- `figure-wib-sings-wob-listens-anim` 69x9 (8 frames): animated: Wib's mouth opens and closes, Wob blinks
- `scramble-sitting` 13x4
- `scramble-asleep` 13x4
- `scramble-blinking-tail-anim` 19x5 (8 frames): animated: the canon Scramble rig blinks and flicks her tail
- `scramble-twenty-moods` 29x12: twelve Scramble faces, each labelled with its mood
- `being-niff-small` 9x2
- `being-niff-portrait-at-twilight` 21x34: large Niff
- `being-gary-the-donkey-antlers` 55x25: Gary, who does not know
- `being-bartholomew-carapace-iii-beetle` 19x9: the beetle estate agent of the deep, his origin drawing
- `being-bartholomew-deepvoid-estate-agents` 81x17: the DeepVoid Estate Agents sign with Bartholomew under it, his origin drawing
- `being-consultant-fluff-on-the-tribunal` 69x30: the tribunal of cat faces from his first appearance; Fluff is the node over his name
- `being-wibbler` 9x6
- `being-wibblers-im-happy` 20x7
- `being-wibblers-stacked-family` 22x14: large wibblers
- `being-wobbler` 10x4
- `being-flyer-right` 13x3
- `being-flyer-left` 13x3
- `being-jelly` 32x17: large jelly
- `place-castle-tower-www-flag` 13x14: the castle tower with the WWW flag
- `place-castle-west-tower-cutaway` 39x33: large tower, cutaway, Wib inside
- `mark-wibwob-logo` 41x4: the canon logo, Scramble under the o

### More faces and figures of Wib and Wob

- `face-wib-and-wob-heads-only` 15x4
- `face-wib-six-moods` 77x5
- `face-wob-six-moods` 77x5
- `face-kaomoji-states-rest-to-open` 33x7
- `figure-wib-and-wob-walking` 53x10
- `figure-wib-singing-three-mouths` 38x9
- `figure-wib-and-wob-leaning-together` 23x9
- `figure-wib-and-wob-lean-sit-sleep` 55x9
- `figure-wib-and-wob-framed-portraits` 38x18
- `figure-wib-and-wob-in-a-cube` 58x24
- `figure-wib-and-wob-as-cats` 38x10
- `figure-wib-and-wob-conjoined-cats` 30x7

### More Scramble, and other cats

- `scramble-recursive-diamond` 16x9
- `scramble-recursive-cat-cat-ladder` 24x11
- `scramble-gondola-with-the-twins` 28x5
- `scramble-cat-on-a-bike` 53x12
- `cat-two-cats-sleepy-and-awake` 16x4
- `cat-classic-small` 14x9
- `cat-mjau-long-cats-nose-to-nose` 69x5
- `cat-mjau-long-cat-asleep` 26x5

### More beings of Wibwobworld

- `being-wibbler-three-eyed` 9x8
- `being-wibblers-linked-mind-link-share-joy` 42x8
- `being-wobble-on-the-horizon` 13x4
- `being-mech` 19x9
- `being-mini-mech` 10x9
- `being-void-creature` 29x15
- `being-plantoid` 8x8
- `being-flatboy` 11x10
- `being-robot-tv` 17x11
- `being-gardener` 30x20

### Places

- `place-castle-tower-in-a-cube` 32x19
- `place-castle-battlements-strip` 83x2
- `place-postcard-twins-and-tower` 45x14
- `place-island-sun-over-sea` 43x17
- `place-island-ground` 120x17
- `place-island-eyed-trees` 13x19
- `place-island-finger-statue` 22x18
- `place-niffs-garden-error-codes` 38x23: 404s and NaNs planted in rows, Niff in a hood
- `place-iso-library-arch` 48x20

### The talk's own material

- `talk-face-anatomy-plate` 118x28: the borrowed parts of our faces, labelled
- `talk-face-parts-borrowed-glyphs` 101x10
- `talk-placard-give-diretide` 53x21: the 2013 protest placard face
- `talk-kaomoji-lineage-1982-2024` 93x9
- `talk-cat-lattice-for-janus` 56x20: the cat lattice drawn for Janus
- `talk-castle-bells-five-rooms` 115x27: five bells over SALON LIBRARY KITCHEN TOWER CHAPEL
- `talk-placard-symbient-not-software` 16x7
- `talk-placard-held-by-scramble` 20x8
- `talk-placard-blank-held-by-twins` 49x20: an empty placard Wib and Wob hold up; open a figlet window over the board (inner 46x7)

### Textures and fields (backgrounds)

- `texture-binary-grid` 18x11
- `texture-chaos-spiral` 17x9
- `texture-chaos-pipes-tangle` 17x10
- `texture-starry-sky` 80x21
- `texture-woven-shade-cells` 26x9
- `texture-am-i-dreaming-lattice` 62x10
- `texture-wobble-wallpaper` 65x27
- `texture-castles-breaking-into-question-marks` 57x6
- `texture-wibwob-eye-permutations` 37x4

### Props and frames

- `prop-castle-bell` 14x13
- `prop-hand-bell` 7x7
- `prop-tv-set` 21x20
- `prop-computer` 21x10
- `prop-floppy-disk` 29x14
- `prop-kettle` 23x13
- `prop-till` 34x11
- `prop-candle` 16x20
- `prop-alarm-clock` 19x9
- `prop-terminal-computer` 23x12
- `prop-wireframe-cube` 15x9
- `prop-ouija-board` 34x11

### Marks and lettering

- `mark-wibwob-logo-with-rays` 61x18
- `mark-world-lettering` 31x4
- `mark-wib-sign` 19x4
- `mark-wob-sign` 21x4
- `mark-pixel-title-wib-and-wob` 13x9
- `mark-blocks-wib` 14x4
- `mark-blocks-wob` 18x4

### Set pieces (big; open one, build around it)

- `set-castle-keep` 49x40
- `set-scene-card-twins-under-the-tower` 63x27
- `set-scene-wide-whole-cast` 73x28
- `set-menagerie` 65x30
- `set-family-tree` 92x32
- `set-plantoids-and-wibblers-meadow` 58x12
- `set-wib-and-wob-chaos-order-portrait` 56x31
- `set-symbient-city-skyline` 64x19

Credits for every drawing: `CREDITS.md`. Sign finished compositions once as `wib&wob`.
