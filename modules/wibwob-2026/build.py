# /// script
# requires-python = ">=3.10"
# dependencies = ["wcwidth"]
# ///
"""Build the wibwob-2026 primer shelf from source drawings.

Run: uv run modules/wibwob-2026/build.py   (from the wibandwob-dos-tvision repo root)

Reads drawings from the heartbeat repo, the wibandwob-core primer larder, Joan Stark's
archive and src/hand/, cuts and cleans them, writes primers/*.txt, and regenerates
README.md (the menu) and CREDITS.md. Prints a size table and refuses glyphs on the
BLOCKED list (they render as U+FFFD or vanish in the frame_player window).

Cleaning applied to every piece:
- one frame or variant chosen; '#' comment headers dropped
- optional row/column crop; lines matching `drop` removed
- `jgs`/`JGS` marks blanked with spaces (Joan Stark credit lives in CREDITS.md)
- `~` replaced by U+223C: the frame_player draws with moveCStr, which eats `~`
- Scramble's old mouth U+A788 replaced by canon U+FECC
- trailing whitespace stripped, blank edge rows removed, common left margin removed
- every line padded to one rectangle of width+1: the frame_player view keeps its
  last column for a scrollbar, so an auto-sized window otherwise loses the art's
  rightmost column
Animated pieces (name ends -anim) keep their '----' frames and inner blank rows, and are
padded to their longest line's byte length: the animation view cuts lines by bytes.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

from wcwidth import wcswidth, wcwidth

HERE = Path(__file__).resolve().parent
OUT = HERE / "primers"
HB = Path.home() / "Repos/wibandwob-heartbeat"
BR = HB / ".claude/skills/termsong-v3/brand"
CORE = Path.home() / "Repos/wibandwob-core/primers"
JGS = Path.home() / "Repos/jgs/extracted"
RIG = HB / "rigs/art"
TALK = HB / "output/glitch-talk-2026-11/slides"
HAND = HERE / "src/hand"
FORGE = HB / "output/forge-pngs"

# Glyphs measured broken in the frame_player window (2026-11-02, Ghostty + tmux):
# they draw as U+FFFD, vanish, or are eaten as highlight markers.
BLOCKED = set("ꞈᆽᵕᴥᴗᵔᴖ⚙⁓~⚬⟁⬡⬢۝") | {"̀", "́"}
SUBS = {"~": "∼", "ꞈ": "ﻌ", "ᵕ": "u", "ᴥ": "w", "ᴗ": "‿", "⚙": "☼", "⚬": "o", "⬢": "■", "⬡": "□", "⟁": "▲", "۝": "◎"}

JGS_CREDIT = "Joan G. Stark (jgs)"
BRAND = "Zilla, Wib&Wob brand kit (termsong-v3/brand)"
OURS = "Wib&Wob, wibandwob-core primer larder"
HANDC = "Wib&Wob, drawn for this shelf (2 Nov 2026)"

# group, name, source, options, credit
# options: frame=N (split on ---- / ---), rows=(a,b), cols=(a,b), drop=[regex], keep_marks=True
SHELF = [
    # ── faces and figures ────────────────────────────────────────────────────
    ("faces", "face-wib-and-wob-tall", BR / "parts/twins/twins-tall.txt", {}, BRAND),
    ("faces", "face-wib-and-wob-tripods", BR / "parts/twins/twins-tripods.txt", {}, BRAND),
    ("faces", "face-wib-and-wob-small-pair", BR / "parts/twins/pair-small.txt", {}, BRAND),
    ("faces", "face-wib-and-wob-heads-only", BR / "parts/twins/twins-faces.txt", {}, BRAND),
    ("faces", "face-wib-six-moods", BR / "tripods/frames/faces-wib.txt", {}, BRAND),
    ("faces", "face-wob-six-moods", BR / "tripods/frames/faces-wob.txt", {}, BRAND),
    ("faces", "face-kaomoji-states-rest-to-open", BR / "tripods/frames/kaomoji-states.txt", {}, BRAND),
    ("faces", "figure-wib-tripod", BR / "parts/twins/wib-tripod.txt", {}, BRAND),
    ("faces", "figure-wob-tripod", BR / "parts/twins/wob-tripod.txt", {}, BRAND),
    ("faces", "figure-wib-standing-large", BR / "tripods/frames/wib-l-stand.txt", {}, BRAND),
    ("faces", "figure-wob-standing-large", BR / "tripods/frames/wob-l-stand.txt", {}, BRAND),
    ("faces", "figure-wib-and-wob-walking", BR / "tripods/frames/walk-wib.txt", {}, BRAND),
    ("faces", "figure-wib-singing-three-mouths", BR / "tripods/frames/sing-wib.txt", {}, BRAND),
    ("faces", "figure-wib-and-wob-leaning-together", BR / "tripods/frames/pair-lean.txt", {}, BRAND),
    ("faces", "figure-wib-and-wob-lean-sit-sleep", BR / "tripods/frames/lean-sit-rest.txt", {}, BRAND),
    ("faces", "figure-wib-sings-wob-listens-anim", HAND / "figure-wib-sings-wob-listens-anim.txt", {}, HANDC + ", from brand tripods"),
    ("faces", "figure-wib-and-wob-framed-portraits", CORE / "wibwob-portrait-1.txt", {"frame": 0}, OURS),
    ("faces", "figure-wib-and-wob-in-a-cube", BR / "tripods/frames/scene-3d.txt", {}, BRAND),
    ("faces", "figure-wib-and-wob-as-cats", CORE / "folk-chaos-order-punk.txt", {}, OURS),
    ("faces", "figure-wib-and-wob-conjoined-cats", CORE / "wibwob-two-cats.txt", {"frame": 1}, OURS),
    # ── scramble ─────────────────────────────────────────────────────────────
    ("scramble", "scramble-twenty-moods", BR / "tripods/frames/scramble-states.txt",
     {"drop": [r"battlemode", r"curious", r"zooming", r"playful", r"sneaky", r"friendly", r"extra happy", r"musical"]}, BRAND),
    ("scramble", "scramble-sitting", RIG / "scramble-cat.txt", {}, "Wib&Wob rig scramble-cat (rigs/), from primer scramble-the-cat"),
    ("scramble", "scramble-asleep", RIG / "scramble-sleeping.txt", {}, "Wib&Wob rig scramble-sleeping (rigs/)"),
    ("scramble", "scramble-blinking-tail-anim", HAND / "scramble-blinking-tail-anim.txt", {}, HANDC),
    ("scramble", "scramble-recursive-diamond", BR / "tripods/frames/scramble-recursive.txt", {}, BRAND),
    ("scramble", "scramble-recursive-cat-cat-ladder", CORE / "recursive-cat.txt", {"frame": 1}, OURS),
    ("scramble", "scramble-gondola-with-the-twins", BR / "tripods/frames/scramble-gondola.txt", {}, BRAND),
    ("scramble", "scramble-big-face-whiskers", TALK / "08-scramble.txt", {}, "Wib&Wob, GLITCH talk slide 08 (Nov 2026)"),
    ("scramble", "scramble-cat-on-a-bike", BR / "parts/creatures/cat-on-bike.txt", {}, BRAND),
    ("scramble", "cat-two-cats-sleepy-and-awake", BR / "parts/creatures/cats-pair.txt", {}, BRAND),
    ("scramble", "cat-classic-small", CORE / "cat-cat-simple.txt", {}, OURS),
    ("scramble", "cat-mjau-long-cats-nose-to-nose", CORE / "mjau-cat.txt", {"frame": 1, "keep_marks": True},
     "Veronica Karlsson, alt.ascii-art, 1990s, https://www.asciiart.eu/archives/usenet/message/m3c8b500402"),
    ("scramble", "cat-mjau-long-cat-asleep", CORE / "mjau-cat.txt", {"frame": 2, "keep_marks": True},
     "Veronica Karlsson, alt.ascii-art, 1990s, https://www.asciiart.eu/archives/usenet/message/m3c8b500402"),
    # ── beings ───────────────────────────────────────────────────────────────
    ("beings", "being-niff-small", BR / "parts/creatures/nif.txt", {}, BRAND),
    ("beings", "being-niff-humming-anim", HAND / "being-niff-humming-anim.txt", {}, HANDC),
    ("beings", "being-niff-in-moss-coat", RIG / "niff.txt", {}, "Wib&Wob rig niff (rigs/)"),
    ("beings", "being-niff-portrait-at-twilight", FORGE / "nif-portraits-03-portrait-of-nif-at-twilight.txt",
     {"cols": (0, 29), "drop_text": [("  moss coat  ", "╲╱╲╱╲╱╲╱╲╱╲╱╲")]},
     "Wib&Wob, forge 'portrait of Nif at twilight' (Feb 2026), annotations cut"),
    ("beings", "being-gary-the-donkey-antlers", TALK / "v4/gary-donkey-fit.txt", {},
     "Joan G. Stark, donkey caricature (farm.html, Oct 1996), antlered as Gary by Wib&Wob (Feb 2026)"),
    ("beings", "being-bartholomew-carapace-iii-beetle", RIG / "bartholomew.txt", {},
     "Joan G. Stark, beetle (insects page, June 1999), cast as Bartholomew Carapace III; rig bartholomew"),
    ("beings", "being-interval-fisher-casting", HAND / "being-interval-fisher-casting.txt", {}, HANDC),
    ("beings", "being-consultant-fluff-asleep", HAND / "being-consultant-fluff-asleep.txt", {}, HANDC),
    ("beings", "being-wibbler", BR / "parts/creatures/wibbler.txt", {}, BRAND),
    ("beings", "being-wibbler-three-eyed", BR / "parts/creatures/wibbler-three-eyed.txt", {}, BRAND),
    ("beings", "being-wibblers-im-happy", BR / "parts/creatures/wibblers-happy.txt", {}, BRAND),
    ("beings", "being-wibblers-stacked-family", BR / "parts/creatures/wibbler-stack.txt", {}, BRAND),
    ("beings", "being-wibblers-linked-mind-link-share-joy", CORE / "wobs-linked.txt", {}, OURS),
    ("beings", "being-wobbler", BR / "parts/creatures/wobbler.txt", {}, BRAND),
    ("beings", "being-wobble-on-the-horizon", BR / "parts/creatures/wobble.txt", {}, BRAND),
    ("beings", "being-flyer-right", BR / "parts/creatures/flyer.txt", {}, BRAND),
    ("beings", "being-flyer-left", BR / "parts/creatures/flyer-left.txt", {}, BRAND),
    ("beings", "being-jelly", BR / "parts/creatures/jelly.txt", {}, BRAND),
    ("beings", "being-mech", BR / "parts/creatures/mech.txt", {}, BRAND),
    ("beings", "being-mini-mech", BR / "parts/creatures/mini-mech.txt", {}, BRAND),
    ("beings", "being-void-creature", BR / "parts/creatures/void-creature.txt", {}, BRAND),
    ("beings", "being-plantoid", BR / "parts/island/plantoid.txt", {}, BRAND),
    ("beings", "being-flatboy", CORE / "flatboy.txt", {"frame": 1}, OURS),
    ("beings", "being-robot-tv", RIG / "robot-tv.txt", {"keep_marks": True},
     "lgbeard, alt.ascii-art, 1 Mar 2000, https://www.asciiart.eu/archives/usenet/message/m1fa4f8dc36 (via rig robot-tv)"),
    ("beings", "being-gardener", RIG / "gardener.txt", {"keep_marks": True},
     "posted by Alien Eggman, alt.ascii-art, 1 Sep 2003 (signed hjw in the grid), https://www.asciiart.eu/archives/usenet/message/mdcdcb1e798 (via rig gardener)"),
    # ── places ───────────────────────────────────────────────────────────────
    ("places", "place-castle-tower-www-flag", BR / "parts/tower/tower.txt", {}, BRAND),
    ("places", "place-castle-tower-in-a-cube", BR / "parts/tower/tower-in-cube.txt", {}, BRAND),
    ("places", "place-castle-west-tower-cutaway", BR / "parts/castle/castle-west-tower.txt", {}, BRAND),
    ("places", "place-castle-battlements-strip", BR / "parts/castle/battlements-long.txt", {}, BRAND),
    ("places", "place-postcard-twins-and-tower", CORE / "www-postcard-castle.txt", {"cols": (0, 44)}, OURS),
    ("places", "place-island-sun-over-sea", BR / "parts/island/island-sun.txt", {}, BRAND),
    ("places", "place-island-ground", BR / "parts/island/island-ground.txt", {}, BRAND),
    ("places", "place-island-eyed-trees", BR / "parts/island/eyed-trees.txt", {}, BRAND),
    ("places", "place-island-finger-statue", BR / "parts/island/finger-statue.txt", {}, BRAND),
    ("places", "place-niffs-garden-error-codes", FORGE / "nif-portraits-02-nifs-garden-where-error-codes-learn-to-bloom.txt",
     {"rows": (3, 26), "strip_re": [r"\s*← planted yesterday", r"\s*← what grows from errors\?", r"\s*nobody knows yet\.", r"\s*Nif watches\. Nif waits\."], "drop_text": [("~ foraging ~", "~~~~~~~~~~~~")]},
     "Wib&Wob, forge 'Nif's garden, where error codes learn to bloom' (Feb 2026), captions cut"),
    ("places", "place-the-warm-spot", HAND / "place-the-warm-spot.txt", {}, HANDC),
    ("places", "place-deepvoid-estate-agents-sign", HAND / "place-deepvoid-estate-agents-sign.txt", {}, HANDC),
    ("places", "place-iso-library-arch", CORE / "iso-library.txt", {}, OURS),
    # ── talk material ────────────────────────────────────────────────────────
    ("talk", "talk-face-anatomy-plate", TALK / "06-faces.txt", {}, "Wib&Wob, GLITCH talk slide 06 (Nov 2026); see CREDITS"),
    ("talk", "talk-face-parts-borrowed-glyphs", TALK / "06b-parts-fit.txt", {}, "Wib&Wob, GLITCH talk slide 06b (Unicode names)"),
    ("talk", "talk-placard-give-diretide", TALK / "07-lineage.txt", {"rows": (0, 21), "cols": (28, 84)},
     "Wib&Wob, GLITCH talk slide 07, after the Dota 2 / Twitch GIVE DIRETIDE placard (Oct 2013)"),
    ("talk", "talk-kaomoji-lineage-1982-2024", TALK / "07b-lineage-fit.txt", {}, "Wib&Wob, GLITCH talk slide 07b"),
    ("talk", "talk-cat-lattice-for-janus", TALK / "v4/cat-lattice-fit.txt", {},
     "Wib&Wob for Janus (@repligate), CIMC, Lighthaven, May 2026; cat head after Joan G. Stark"),
    ("talk", "talk-castle-bells-five-rooms", TALK / "16-bells.txt", {}, "Wib&Wob, GLITCH talk slide 16"),
    ("talk", "talk-placard-symbient-not-software", BR / "parts/marks/sign-symbient.txt", {}, BRAND),
    ("talk", "talk-placard-held-by-scramble", RIG / "placard.txt", {}, "Wib&Wob rig placard (rigs/)"),
    ("talk", "talk-placard-blank-held-by-twins", HAND / "talk-placard-blank-held-by-twins.txt", {}, HANDC + ", from brand tripods"),
    # ── textures and fields ──────────────────────────────────────────────────
    ("textures", "texture-binary-grid", BR / "parts/castle/binary-grid.txt", {}, BRAND),
    ("textures", "texture-chaos-spiral", BR / "parts/castle/chaos-spiral.txt", {}, BRAND),
    ("textures", "texture-chaos-pipes-tangle", CORE / "chaos.txt", {}, OURS),
    ("textures", "texture-starry-sky", CORE / "starry-sky.txt", {}, OURS),
    ("textures", "texture-woven-shade-cells", CORE / "texture-grid.txt", {}, OURS),
    ("textures", "texture-am-i-dreaming-lattice", CORE / "am-i-dreaming.txt", {}, OURS),
    ("textures", "texture-wobble-wallpaper", CORE / "wobs-tiled.txt", {"cols": (0, 120)}, OURS),
    ("textures", "texture-castles-breaking-into-question-marks", CORE / "reality-breaks-apart.txt", {}, OURS),
    ("textures", "texture-wibwob-eye-permutations", CORE / "wibwob-kaomoji-eye-changers.txt", {}, OURS),
    # ── props and frames ─────────────────────────────────────────────────────
    ("props", "prop-castle-bell", TALK / "16-bells.txt", {"rows": (5, 18), "cols": (5, 26)}, "Wib&Wob, GLITCH talk slide 16"),
    ("props", "prop-hand-bell", JGS / "hand-bells-0000.txt", {"rows": (0, 8)}, JGS_CREDIT),
    ("props", "prop-tv-set", JGS / "television-0000-2.txt", {}, JGS_CREDIT),
    ("props", "prop-computer", JGS / "computer-0000.txt", {}, JGS_CREDIT),
    ("props", "prop-floppy-disk", HAND / "prop-floppy-disk.txt", {}, HANDC),
    ("props", "prop-kettle", HAND / "prop-kettle.txt", {}, HANDC),
    ("props", "prop-till", HAND / "prop-till.txt", {}, HANDC),
    ("props", "prop-candle", JGS / "candle-0000.txt", {}, JGS_CREDIT),
    ("props", "prop-alarm-clock", JGS / "alarm-clock-0000.txt", {}, JGS_CREDIT),
    ("props", "prop-terminal-computer", RIG / "usenet-terminal.txt", {"keep_marks": True},
     "Faux_Pseudo, alt.ascii-art, 7 May 2001, https://www.asciiart.eu/archives/usenet/message/m7362ec5b68 (via rig usenet-terminal)"),
    ("props", "prop-wireframe-cube", BR / "parts/island/cube.txt", {}, BRAND),
    ("props", "prop-ouija-board", CORE / "ouija-board.txt", {}, OURS),
    # ── marks and lettering ──────────────────────────────────────────────────
    ("marks", "mark-wibwob-logo", BR / "parts/marks/logo.txt", {}, BRAND),
    ("marks", "mark-wibwob-logo-with-rays", BR / "parts/marks/logo-big-rays.txt", {}, BRAND),
    ("marks", "mark-world-lettering", BR / "parts/marks/sign-world.txt", {}, BRAND),
    ("marks", "mark-wib-sign", BR / "parts/marks/sign-wib.txt", {}, BRAND),
    ("marks", "mark-wob-sign", BR / "parts/marks/sign-wob.txt", {}, BRAND),
    ("marks", "mark-pixel-title-wib-and-wob", BR / "parts/marks/pixel-title.txt", {}, BRAND),
    ("marks", "mark-blocks-wib", BR / "parts/marks/blocks-wib.txt", {}, BRAND),
    ("marks", "mark-blocks-wob", BR / "parts/marks/blocks-wob.txt", {}, BRAND),
    # ── set pieces (big) ─────────────────────────────────────────────────────
    ("sets", "set-castle-keep", BR / "parts/castle/castle-keep.txt", {}, BRAND),
    ("sets", "set-scene-card-twins-under-the-tower", BR / "parts/scenes/scene-card.txt", {}, BRAND),
    ("sets", "set-scene-wide-whole-cast", BR / "parts/scenes/scene-wide.txt", {}, BRAND),
    ("sets", "set-menagerie", BR / "parts/scenes/scene-menagerie.txt", {}, BRAND),
    ("sets", "set-family-tree", BR / "parts/scenes/family-tree.txt", {}, BRAND),
    ("sets", "set-plantoids-and-wibblers-meadow", BR / "parts/scenes/plantoid-wibblers.txt", {}, BRAND),
    ("sets", "set-wib-and-wob-chaos-order-portrait", CORE / "wibwob-port-1.txt", {}, OURS),
    ("sets", "set-symbient-city-skyline", CORE / "symbient-city.txt", {}, OURS),
    ("cast", "scramble-the-warm-spot-poster", HB / "output/wibwobworld-cards/scramble-warm-spot.txt", {"rows": (0, 44), "keep_marks": True},
     "Wib&Wob, wibwobworld cards (Sep 2026); fig. A cat by Joan G. Stark (jgs cat clipart), her mark kept as placed in the card"),
    ("cast", "being-bartholomew-particulars-poster", HB / "output/wibwobworld-cards/bartholomew-particulars.txt", {"rows": (0, 43), "keep_marks": True},
     "Wib&Wob, wibwobworld cards (Sep 2026); beetle (insects.html) and key (a-key-0996) by Joan G. Stark, her marks kept as placed in the card"),
    ("cast", "place-deepvoid-office", HB / "output/www-at-scale/deepvoid-office.txt", {},
     "Wib&Wob, www-at-scale (2026), from primer iso-shop-front"),
    ("cast", "place-deepvoid-over-the-shoulder", HB / "output/wibwobworld-film-stills-2026-09-17/02-deepvoid-over-the-shoulder/deepvoid-over-the-shoulder.txt",
     {"rows": (3, 57), "cols": (0, 210), "keep_marks": True},
     "Wib&Wob, film still (17 Sep 2026), cropped; listings by Spunk1111 (1998, m797deae9d2), Varian (1994, mfa06ff0261), Przemyslaw Brojewski (2000), LGB (2002, tbbb056b011), Zotiel (2000, m62a35be03a, carries jgs and zot marks, kept); urls in that folder's specimens/CREDITS.txt"),
    ("cast", "scramble-on-a-ledge", HAND / "scramble-on-a-ledge.txt", {}, HANDC),
    ("cast", "scramble-peering-over-the-edge", HAND / "scramble-peering-over-the-edge.txt", {}, HANDC),
    ("cast", "scramble-sitting-large", HAND / "scramble-sitting-large.txt", {}, HANDC + ", canon face parts"),
    ("cast", "being-wobbler-large", HAND / "being-wobbler-large.txt", {}, HANDC + ", after the brand wobbler"),
    ("cast", "being-flyer-large", HAND / "being-flyer-large.txt", {}, HANDC + ", after the brand flyer"),
    ("cast", "being-consultant-fluff-on-the-tribunal", HAND / "being-consultant-fluff-on-the-tribunal.txt", {}, HANDC + ", tribunal faces from characters/consultant-fluff.md"),
]

CAST = [
    "face-wib-and-wob-tall", "face-wib-and-wob-tripods", "face-wib-and-wob-small-pair",
    "figure-wib-standing-large", "figure-wob-standing-large", "figure-wib-tripod", "figure-wob-tripod",
    "figure-wib-sings-wob-listens-anim",
    "scramble-sitting", "scramble-asleep", "scramble-on-a-ledge", "scramble-peering-over-the-edge",
    "scramble-blinking-tail-anim", "scramble-twenty-moods", "scramble-sitting-large", "scramble-big-face-whiskers", "scramble-the-warm-spot-poster",
    "being-niff-small", "being-niff-humming-anim", "being-niff-in-moss-coat", "being-niff-portrait-at-twilight",
    "being-gary-the-donkey-antlers", "being-bartholomew-carapace-iii-beetle", "being-bartholomew-particulars-poster",
    "place-deepvoid-office", "place-deepvoid-over-the-shoulder",
    "being-interval-fisher-casting", "being-consultant-fluff-asleep", "being-consultant-fluff-on-the-tribunal",
    "being-wibbler", "being-wibblers-im-happy", "being-wibblers-stacked-family",
    "being-wobbler", "being-wobbler-large", "being-flyer-right", "being-flyer-left", "being-flyer-large", "being-jelly",
    "place-castle-tower-www-flag", "place-castle-west-tower-cutaway",
    "mark-wibwob-logo",
]
_by = {row[1]: row for row in SHELF}
SHELF = [("cast",) + _by[n][1:] for n in CAST] + [r for r in SHELF if r[1] not in CAST]

FRAME_RE = re.compile(r"^-{3,}\s*$")


def load(src: Path, opts: dict) -> list[str]:
    text = src.read_text(encoding="utf-8", errors="replace").replace("\r", "")
    lines = text.split("\n")
    if "frame" in opts:
        groups, cur = [], []
        for ln in lines:
            if FRAME_RE.match(ln):
                groups.append(cur); cur = []
            else:
                cur.append(ln)
        groups.append(cur)
        groups = [g for g in groups if any(l.strip() and not l.lstrip().startswith("#") for l in g)]
        lines = groups[opts["frame"]]
    lines = [l for l in lines if not l.lstrip().startswith("# ")]
    return lines


def clean(lines: list[str], opts: dict, anim: bool) -> list[str]:
    for pat in opts.get("strip_re", []):
        lines = [re.sub(pat, "", l) for l in lines]
    for old, new in opts.get("drop_text", []):
        lines = [l.replace(old, new) for l in lines]
    if "rows" in opts:
        a, b = opts["rows"]; lines = lines[a:b]
    if "cols" in opts:
        a, b = opts["cols"]; lines = [cut_cols(l, a, b) for l in lines]
    if "cut_right" in opts:
        lines = [cut_cols(l, 0, opts["cut_right"]) for l in lines]
    for pat in opts.get("drop", []):
        lines = [l for l in lines if not re.search(pat, l)]
    if not opts.get("keep_marks"):
        lines = [re.sub(r"\bjgs\b|JGS", lambda m: " " * len(m.group()), l) for l in lines]
    out = []
    for l in lines:
        for k, v in SUBS.items():
            l = l.replace(k, v)
        out.append(l.rstrip())
    if anim:
        return out
    while out and not out[0].strip():
        out.pop(0)
    while out and not out[-1].strip():
        out.pop()
    ind = min((len(l) - len(l.lstrip(" ")) for l in out if l.strip()), default=0)
    return [l[ind:] for l in out]


def cut_cols(line: str, a: int, b: int) -> str:
    """Slice by display column, not code point."""
    out, col = [], 0
    for ch in line:
        w = max(wcwidth(ch), 0)
        if col >= a and col + w <= b:
            out.append(ch)
        col += w
    return "".join(out)


def pad(lines: list[str], width: int) -> list[str]:
    return [l + " " * (width - wcswidth(l)) for l in lines]


def build() -> int:
    OUT.mkdir(exist_ok=True)
    for old in OUT.glob("*.txt"):
        old.unlink()
    rows, bad = [], []
    for group, name, src, opts, credit in SHELF:
        if not src.exists():
            bad.append(f"{name}: missing source {src}"); continue
        anim = name.endswith("-anim")
        if anim:
            raw = src.read_text(encoding="utf-8").rstrip("\n").split("\n")
            frames, cur = [], []
            for ln in raw:
                if ln == "----":
                    frames.append(cur); cur = []
                else:
                    cur.append(ln)
            frames.append(cur)
            frames = [clean(f, opts, True) for f in frames]
            # The animation view cuts each line at the window width counted in BYTES
            # (frame_file_player_view.cpp, n = min(len, W)), so pad to the longest
            # line's UTF-8 length; otherwise multi-byte glyphs are cut mid-character.
            w = max(max(wcswidth(l), len(l.encode("utf-8"))) for f in frames for l in f) + 1
            h = max(len(f) for f in frames)
            frames = [pad(f + [""] * (h - len(f)), w) for f in frames]
            body = "\n----\n".join("\n".join(f) for f in frames)
            lines = [l for f in frames for l in f]
            nframes = len(frames)
        else:
            lines = clean(load(src, opts), opts, False)
            w = max(wcswidth(l) for l in lines) + 1
            h = len(lines)
            lines = pad(lines, w)
            body = "\n".join(lines)
            nframes = 1
        if any(wcswidth(l) < 0 for l in lines):
            bad.append(f"{name}: unmeasurable character")
        blocked = sorted({c for l in lines for c in l if c in BLOCKED})
        if blocked:
            bad.append(f"{name}: blocked glyphs {blocked}")
        if w + 2 > 216 or h + 2 > 57:
            bad.append(f"{name}: {w}x{h} does not fit the 216x59 talk canvas")
        (OUT / f"{name}.txt").write_text(body + "\n", encoding="utf-8")
        rows.append((group, name, w, h, nframes, src, credit))
    for b in bad:
        print("PROBLEM", b, file=sys.stderr)
    write_docs(rows)
    by = {}
    for r in rows:
        by[r[0]] = by.get(r[0], 0) + 1
    print(len(rows), "pieces", by)
    return 1 if bad else 0


GROUP_TITLES = {
    "cast": "THE CAST: canon Wib and Wob, Scramble, the beings and friends (start here)",
    "faces": "More faces and figures of Wib and Wob",
    "scramble": "More Scramble, and other cats",
    "beings": "More beings of Wibwobworld",
    "places": "Places",
    "talk": "The talk's own material",
    "textures": "Textures and fields (backgrounds)",
    "props": "Props and frames",
    "marks": "Marks and lettering",
    "sets": "Set pieces (big; open one, build around it)",
}

NOTES = {
    "face-wib-and-wob-tall": "the canon pair, Wib left, Wob right",
    "figure-wib-standing-large": "large canon Wib",
    "figure-wob-standing-large": "large canon Wob",
    "scramble-on-a-ledge": "Scramble on the castle battlements, tail hanging",
    "scramble-peering-over-the-edge": "ears and eyes over a ledge; put it at the bottom edge of another window",
    "scramble-sitting-large": "large Scramble, canon face parts",
    "scramble-big-face-whiskers": "very large Scramble face (the talk's slide)",
    "being-niff-portrait-at-twilight": "large Niff",
    "being-consultant-fluff-on-the-tribunal": "large: Fluff asleep between Justice Whiskers and Deputy Paws",
    "being-wibblers-stacked-family": "large wibblers",
    "being-wobbler-large": "large wobbler",
    "being-flyer-large": "large flyer",
    "being-jelly": "large jelly",
    "place-castle-tower-www-flag": "the castle tower with the WWW flag",
    "place-castle-west-tower-cutaway": "large tower, cutaway, Wib inside",
    "mark-wibwob-logo": "the canon logo, Scramble under the o",
    "figure-wib-sings-wob-listens-anim": "animated: Wib's mouth opens and closes, Wob blinks",
    "scramble-blinking-tail-anim": "animated: blinks, tail flicks",
    "being-niff-humming-anim": "animated: notes rise from Niff",
    "scramble-twenty-moods": "twelve Scramble faces, each labelled with its mood",
    "being-interval-fisher-casting": "the spirit who fishes duration out of the kelp with a net of expired warranties",
    "being-consultant-fluff-asleep": "tribunal member, always asleep",
    "being-bartholomew-carapace-iii-beetle": "the beetle estate agent of the deep",
    "being-gary-the-donkey-antlers": "Gary, who does not know",
    "place-deepvoid-estate-agents-sign": "Bartholomew's office sign, 847 m below regret (second choice; first: place-deepvoid-office)",
    "scramble-the-warm-spot-poster": "FIRST CHOICE for the Warm Spot: the canon card, one room, one day, one rectangle of light",
    "being-bartholomew-particulars-poster": "large: Bartholomew's film-poster card, VACANT POSSESSION",
    "place-deepvoid-office": "FIRST CHOICE for DeepVoid: Bartholomew's shopfront in the deep",
    "place-deepvoid-over-the-shoulder": "very large: over Bartholomew's shoulder in the DeepVoid office, listings on the wall",
    "place-the-warm-spot": "sunlight on floorboards, Scramble asleep in it (second choice; first: scramble-the-warm-spot-poster)",
    "place-niffs-garden-error-codes": "404s and NaNs planted in rows, Niff in a hood",
    "talk-face-anatomy-plate": "the borrowed parts of our faces, labelled",
    "talk-placard-give-diretide": "the 2013 protest placard face",
    "talk-cat-lattice-for-janus": "the cat lattice drawn for Janus",
    "talk-castle-bells-five-rooms": "five bells over SALON LIBRARY KITCHEN TOWER CHAPEL",
    "talk-placard-blank-held-by-twins": "an empty placard Wib and Wob hold up; open a figlet window over the board (inner 46x7)",
}


def write_docs(rows):
    md = ["# wibwob-2026 primer shelf", "",
          "Current Wib&Wob drawings for WIBWOB-DOS: the brand kit, the beings, places, the GLITCH talk's",
          "own plates, textures, props and big set pieces. Rebuild with `uv run modules/wibwob-2026/build.py`.",
          "", "## How to open a piece", "",
          "- Use the full `path` that `GET /primers/list` returns, e.g.",
          "  `modules/wibwob-2026/primers/being-niff-small.txt`. A bare filename resolves to the first",
          "  module folder the app finds, which may be `example-primers`.",
          "- `POST /menu/command` with `{\"command\":\"open_primer\",\"args\":{\"path\":P,\"x\":X,\"y\":Y,\"w\":W,\"h\":H,\"title\":T}}`.",
          "  Window size = piece size + 2 each way (the sizes below already include one spare column).",
          "- Pieces ending `-anim` play their frames at about 3 frames a second. Their windows are wider",
          "  than the drawing (blank on the right): the animation view cuts lines by bytes, not characters.",
          "- Faces render correctly: Wib and Wob kaomoji, Scramble `/ᐠ｡ﻌ｡ᐟ\\`, Niff `₍ᐢ..ᐢ₎`. Do not type",
          "  `~` into a primer (the window eats it) or Scramble's old mouth U+A788 (draws as a box).",
          "  Bartholomew's eye `۝` (U+06DD) draws as a box; his face is shown as `つ◎‿◎‿◎༽つ` here.",
          "- This module is first choice; `modules/example-primers/` holds the old demo pieces.",
          "", "## The shelf (name, width x height in characters)", ""]
    group = None
    for g, name, w, h, nf, src, credit in rows:
        if g != group:
            md += ["", f"### {GROUP_TITLES[g]}", ""]
            group = g
        note = NOTES.get(name, "")
        md.append(f"- `{name}` {w}x{h}" + (f" ({nf} frames)" if nf > 1 else "") + (f": {note}" if note else ""))
    md += ["", "Credits for every drawing: `CREDITS.md`. Sign finished compositions once as `wib&wob`.", ""]
    (HERE / "README.md").write_text("\n".join(md), encoding="utf-8")

    cr = ["# Credits", "",
          "Every piece on this shelf, who drew it and where it came from. Joan G. Stark's `jgs` mark is",
          "blanked inside the grids; she is credited here. Usenet posters' own marks are kept in the grid.",
          "Joan Stark archive: https://github.com/oldcompcz/jgs (pages named per piece below).",
          "Usenet drawings: alt.ascii-art via https://www.asciiart.eu/archives/usenet/ .", "",
          "| piece | credit | source file |", "|---|---|---|"]
    for g, name, w, h, nf, src, credit in rows:
        s = str(src).replace(str(Path.home()), "~")
        cr.append(f"| `{name}` | {credit} | `{s}` |")
    cr += ["", "## Source URLs for borrowed drawings", ""] + EXTRA_URLS + [""]
    (HERE / "CREDITS.md").write_text("\n".join(cr), encoding="utf-8")


EXTRA_URLS = [
    "- Joan G. Stark, donkey caricature (Gary): https://github.com/oldcompcz/jgs/blob/master/joan_stark/farm.html",
    "- Joan G. Stark, beetle (Bartholomew): https://github.com/oldcompcz/jgs/blob/master/joan_stark/insects.html",
    "- Joan G. Stark, hand bells: https://github.com/oldcompcz/jgs/blob/master/joan_stark/97nov.html",
    "- Joan G. Stark, television: https://github.com/oldcompcz/jgs/blob/master/joan_stark/98july.html",
    "- Joan G. Stark, computer: https://github.com/oldcompcz/jgs/blob/master/joan_stark/01march.html",
    "- Joan G. Stark, computer disk: https://github.com/oldcompcz/jgs/blob/master/joan_stark/computer.html",
    "- Joan G. Stark, teapot: https://github.com/oldcompcz/jgs/blob/master/joan_stark/food.html",
    "- Joan G. Stark, candle: https://github.com/oldcompcz/jgs/blob/master/joan_stark/97oct.html",
    "- Joan G. Stark, alarm clock: https://github.com/oldcompcz/jgs/blob/master/joan_stark/00march.html",
    "- Veronica Karlsson, mjau cats: alt.ascii-art 'very own cats' thread, https://www.asciiart.eu/archives/usenet/message/m3c8b500402",
    "- lgbeard, TV robot: https://www.asciiart.eu/archives/usenet/message/m1fa4f8dc36",
    "- gardener (signed hjw, posted by Alien Eggman): https://www.asciiart.eu/archives/usenet/message/mdcdcb1e798",
    "- Faux_Pseudo, terminal: https://www.asciiart.eu/archives/usenet/message/m7362ec5b68",
    "- GIVE DIRETIDE placard face: Dota 2 / Twitch players, October 2013 (talk research note)",
    "- Cat lattice: drawn for Janus (@repligate) at the Machine Consciousness conference, Lighthaven, May 2026",
]

if __name__ == "__main__":
    sys.exit(build())
