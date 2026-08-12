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
