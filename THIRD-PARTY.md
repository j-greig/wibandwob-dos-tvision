# Third-party components

LICENSE.md (PolyForm Noncommercial 1.0.0) covers the WibWob-DOS code written for this
repository. The components below are not covered by it. Each stays under its own terms,
listed here with where it lives.

## Turbo Vision (vendor/tvision, git submodule)

Borland's Turbo Vision as ported by magiblot (https://github.com/magiblot/tvision). Its
COPYRIGHT file says:

> Borland International made the Turbo Vision source code public, accompanied
> by the following disclaimer:
>
> DISCLAIMER AND LIMITATION OF LIABILITY: Borland does not make
> or give any representation or warranty with respect to the
> usefulness or the efficiency of this software, [...] THIS SOFTWARE IS
> PROVIDED 'AS IS', WITHOUT EXPRESS OR IMPLIED WARRANTIES, [...]

(full text in vendor/tvision/COPYRIGHT), and:

> The copyright below applies to the modifications made by magiblot and other
> contributors to the original source code and to the source code that has been
> written from scratch.
>
> MIT License
>
> Copyright 2019-2025 magiblot <magiblot@hotmail.com>

followed by the notices of the third-party code that Turbo Vision itself includes.

Changes this repository makes to Turbo Vision's own files are kept as patches in
`kernel/platform/web/patches/tvision/`. Those patches are offered under Turbo Vision's terms
(the MIT licence above), not under LICENSE.md, so they can be given back to that project.

## tvterm and libvterm (vendor/tvterm, git submodule)

Terminal widget by magiblot, MIT licence (vendor/tvterm/COPYRIGHT). It bundles libvterm
(vendor/tvterm/deps/vterm, a nested submodule), MIT licence upstream. Native build only; the browser build does not
compile it.

## Micropolis (vendor/MicropolisCore, git submodule)

The Micropolis engine, GNU GPL 3 or later with additional terms from Electronic Arts
(vendor/MicropolisCore/MicropolisGPLLicenseNotice.md, MicropolisPublicNameLicense.md). The
native build compiles it into the wwdos binary. The browser build does not: app/web/
micropolis_stub.cpp stands in for it.

## xterm.js (kernel/pages/vendor)

@xterm/xterm 5.5.0, @xterm/addon-fit 0.10.0, @xterm/addon-webgl 0.18.0, MIT licence
(kernel/pages/vendor/xterm.LICENSE). Inlined into pages built from kernel/pages/shell.html.

## Emscripten runtime

Every browser page contains JavaScript glue and libc/libc++ code from Emscripten
(https://emscripten.org), available under the MIT licence and the University of
Illinois/NCSA Open Source License, with musl (MIT) and LLVM libc++ (Apache 2.0 with LLVM
exception) inside it.

## GNU Unifont glyphs (kernel/pages/atlas.js)

The default atlas is a bitmap subset of GNU Unifont 16.0.04 (https://unifoundry.com/unifont/),
made by kernel/tools/make_atlas.py. Unifont's fonts are licensed under the GNU GPL version 2
or later with the GNU font embedding exception, and under the SIL Open Font License 1.1. The
atlas carries the same terms. The font file itself is not in this repository.

## FIGlet fonts

None are shipped in this repository. The app calls a FIGlet program installed on the host.

## Drawings and documents

Drawings (the primer library in modules/, paintings/, screenshots) and documents are licensed
under Creative Commons Attribution-NonCommercial-ShareAlike 4.0
(https://creativecommons.org/licenses/by-nc-sa/4.0/) unless a file says otherwise. Drawings
credited to other artists remain theirs and are under the terms their authors set.
