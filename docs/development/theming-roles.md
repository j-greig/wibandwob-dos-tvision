# Skin Roles — theming architecture (tl;dr)

One role vocabulary, one resolver, all consumers read through it. CgaSkin
table (theme_manager.cpp kSkins) stays the single source of truth; SkinRole
is a *view* over it with documented derivations, so adding a skin remains
"add one line". Designed 2026-08-12 (opus agent); full rationale in git
history of this file.

Roles: Desk, Paper, Dialog, Bar, BarSel, FramePassive, FrameActive, Dim,
Accent, Floor, FloorInk, Ok, Warn, Shadow.

API (ThemeManager): attr(role) — safe everywhere, house default when no
skin; tryAttr(role, out) — for mapColor overrides that must preserve
TVision fallbacks; bios(role) — packed BIOS byte for palette patching;
fgIndex/bgIndex/rgbFg/rgbBg(role); attrIdx(fg,bg) — the canonical cga()
helper every view used to reinvent; attrOver(role, fgOverride) — layering
primitive (skin default under, per-window override on top).

Derivations when a CgaSkin field is -1: FramePassive←paper, FrameActive←
dialog, Bar←black-on-white classic, Floor←Desk, Dim←grey-8 on paper,
Accent←dialogFg, Ok←10, Warn←12, Shadow=solid black.

Rule of thumb: if the colour describes a UI affordance, it is a role; if
it describes a thing being depicted (disk art, shader phosphor, gradient
content, loaded ANSI), it is content — do not migrate.

Migration sequencing (each step revertable): 1 roles+resolver; 2 bars+
getPalette+shadow; 3 TCGAFrame; 4 TBackgroundConfig fromSkin flags +
applySkinPaper (per-window overrides survive skin switch + workspace);
5 disk library floor; 6 shader dim / player 0x07 fallbacks / rulers.

Future (out of scope, designed): user skins from ./skins/*.skin key=value
files shadowing built-ins; theme_mode dark = selector over the skin table
(CgaSkin.dark + counterpart), implemented purely via api_set_skin.


## MS-DOS colour canon (research, 2026-08-13)

Extracted from the reference corpus (Pipeline 2.05, Turbo Pascal 7 Colors
dialog, Terra Time 1987, UVL oil game — design/figma-refs/color-combos.png).
VGA text mode: attribute byte = bg<<4 | fg; 16 fg, 8 bg (+bright via
blink-bit repurpose). The conventions that make the era sing:

**Combos with names** (BIOS attr — meaning):
- 0x70 black-on-grey — panel/menu paper, THE dialog default
- 0x71 blue-on-grey  — German-shareware body text ("grey card, blue ink")
- 0x1F white-on-blue — editor/document body (Borland canon)
- 0x1E yellow-on-blue — highlighted values in documents
- 0x30 black-on-cyan / 0x3F — inner panels, list boxes (TP Colors dialog)
- 0x20 black-on-green — BUTTONS (TP), always with solid black shadow
- 0x02/0x0A green-on-black — terminal dialogs, bordered green (UVL drilling)
- 0x74/0x7C red-on-grey — money, credit, warnings inside grey panels
- 0x60 black-on-brown — cargo/ship/wood panels (UVL Schiff)
- 0x0E/0x7E yellow — titles, hotkey letters, meridians, city labels
- 0x0D/0x05 magenta — rare flash accents (Stand/date stamps, UVL labels)

**Structural rules**:
1. VARIETY PER WINDOW: 3-4 window colour identities coexist per screen
   (grey panel, teal panel, brown panel, green dialog). One uniform paper
   is the tell of a modern imitation. → CgaSkin.paperVariants.
2. POLYCHROME INTERIORS: labels one colour, values another, hotkeys a
   third. Text is never monochrome inside a window.
3. Desktops are DITHERED fields (▒ ░), black-with-grey or blue-with-blue.
4. Shadows are solid black, offset right+down, non-negotiable.
5. Selection is INVERSE (swap fg/bg), never a third colour.
6. Accents are scarce: yellow for guidance, magenta/red for alarm —
   never decoration.
