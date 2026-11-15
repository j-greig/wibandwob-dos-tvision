#!/usr/bin/env python3
"""Audit the describe contract (app/describe.h): every window class in the desktop app says what it is.

Lists every class compiled into the wwdos target that derives from TWindow or TDialog, and how each is
described:
  self     the window class derives from ww::Describable
  view     the window inserts a view (new V in its source files) that derives from ww::Describable
  external a describer registered with ww::addExternalDescriber (for classes the film engine shares)
  dialog   a TDialog: described by kind (title and controls, ww::dialogState)
  MISSING  none of these
It also checks that every class a window type's matcher names (window_type_registry.cpp) is covered.
Exit 0 when nothing is missing, 1 otherwise. --json prints the table as JSON.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(sys.argv[sys.argv.index("--root") + 1]) if "--root" in sys.argv else pathlib.Path(__file__).resolve().parent.parent
APP = ROOT / "app"


def wwdos_sources():
    """The .cpp files of the wwdos target (TV_COMMON_SOURCES and add_executable(wwdos ...))."""
    text = (APP / "CMakeLists.txt").read_text()
    files = []
    for block in (r"set\(TV_COMMON_SOURCES(.*?)\)", r"add_executable\(wwdos\b(.*?)\)"):
        m = re.search(block, text, re.S)
        for tok in m.group(1).split():
            if tok.endswith(".cpp") and not tok.startswith("#"):
                files.append(APP / tok)
    return files


TOKEN_RE = re.compile(r'("(?:\\.|[^"\\\n])*")|(\'(?:\\.|[^\'\\\n])+\')|(/\*.*?\*/)|(//[^\n]*)', re.S)


def strip_comments(s):
    """Drop comments; keep string and character literals (a URL's // is not a comment)."""
    return TOKEN_RE.sub(lambda m: m.group(1) or m.group(2) or " ", s)


CLASS_RE = re.compile(r"\bclass\s+(\w+)\s*(?:final\s*)?:\s*([^{;]+)\{")


def classes_in(path):
    out = []
    for m in CLASS_RE.finditer(strip_comments(path.read_text(errors="replace"))):
        bases = [b.strip().split()[-1] for b in m.group(2).split(",")]
        out.append((m.group(1), bases))
    return out


def main():
    sources = wwdos_sources()
    files = set()
    for cpp in sources:
        if not cpp.exists():
            continue
        files.add(cpp)
        h = cpp.with_suffix(".h")
        if h.exists():
            files.add(h)
    # every class in those files, with its bases
    bases = {}
    where = {}
    for f in sorted(files):
        for name, bs in classes_in(f):
            bases.setdefault(name, set()).update(bs)
            where.setdefault(name, set()).add(f)

    def derives(name, target, seen=None):
        seen = seen or set()
        if name in seen:
            return False
        seen.add(name)
        for b in bases.get(name, ()):
            b = b.replace("ww::", "").replace("tvterm::", "")
            if b == target or derives(b, target, seen):
                return True
        return False

    describable = {n for n in bases if derives(n, "Describable")}
    externals = set()
    for f in files:
        externals.update(re.findall(r'addExternalDescriber\(\{\s*"(\w+)"', strip_comments(f.read_text(errors="replace"))))

    def source_of(name):
        text = ""
        for f in where.get(name, ()):
            text += strip_comments(f.read_text(errors="replace"))
            for sib in (f.with_suffix(".cpp"), f.with_suffix(".h")):
                if sib != f and sib.exists():
                    text += strip_comments(sib.read_text(errors="replace"))
        return text

    rows = []
    for name in sorted(bases):
        is_dialog = derives(name, "TDialog")
        is_window = is_dialog or derives(name, "TWindow") or "BasicTerminalWindow" in bases[name]
        if not is_window:
            continue
        how, via = "MISSING", ""
        if name in describable:
            how = "self"
        elif name in externals:
            how = "external"
        elif is_dialog:
            how = "dialog"
        else:
            text = source_of(name)
            found = [v for v in sorted(describable) if re.search(r"\bnew\s+" + re.escape(v) + r"\b", text)]
            if found:
                twin = name.replace("Window", "View")
                how, via = "view", twin if twin in found else found[0]
        rows.append({"class": name, "described": how, "via": via,
                     "file": sorted(str(p.relative_to(ROOT)) for p in where[name])[0]})

    # classes the window type matchers name must be covered too
    reg = strip_comments((APP / "window_type_registry.cpp").read_text())
    matched = set(re.findall(r"(?:dynamic_cast<|has_child_view<)(\w+)\*?>", reg)) - {"ViewType"}
    covered = {r["class"] for r in rows if r["described"] != "MISSING"} | describable
    # a matcher class is also covered when a described window inserts it (or a class derived from it)
    inserted = set()
    for r in rows:
        if r["described"] in ("self", "external"):
            inserted.update(re.findall(r"\bnew\s+(\w+)", source_of(r["class"])))
    unmatched = sorted(m for m in matched if m not in covered
                       and not any(i == m or derives(i, m) for i in inserted))

    missing = [r for r in rows if r["described"] == "MISSING"]
    if "--json" in sys.argv:
        print(json.dumps({"windows": rows, "missing": [r["class"] for r in missing],
                          "matcher_classes_missing": unmatched}, indent=1))
    else:
        for r in rows:
            print(f"{r['described']:8} {r['class']:34} {r['via']:28} {r['file']}")
        print(f"{len(rows)} window classes; {len(missing)} missing; matcher classes missing: {len(unmatched)}")
        for m in unmatched:
            print(f"MISSING  matcher class {m}")
    return 1 if missing or unmatched else 0


if __name__ == "__main__":
    sys.exit(main())
