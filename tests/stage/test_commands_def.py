"""The stage driver's own command lists agree with app/stage/commands.def.

usage: python3 tests/stage/test_commands_def.py      (or pytest tests/stage)

stage.cpp keeps three lists by hand: checkLive's known[] (what window.wwdosStage.run() accepts), notLive[] (what
it refuses) and needWin[] (commands whose first word must name an open window; written twice, in run() and in
checkLive()). Each must equal the matching column of commands.def.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEF = ROOT / "app/stage/commands.def"
CPP = ROOT / "app/stage/stage.cpp"


def read_def(path=DEF):
    rows = {}
    for line in path.read_text().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        head, _, doc = line.partition("|")
        name, kind, mn, win, text, live = head.split()
        assert kind in ("run", "head") and win in "yn" and text in "yn" and live in "yn", line
        rows[name] = dict(kind=kind, min=int(mn), win=win == "y", text=text == "y", live=live == "y", doc=doc.strip())
    return rows


def cpp_lists(name, path=CPP):
    """Every `static const char* const NAME[] = {...}` in stage.cpp, as sets of strings."""
    src = path.read_text()
    found = [set(re.findall(r'"(\w+)"', m)) for m in
             re.findall(r"static const char\* const " + name + r"\[\]\s*=\s*\{(.*?)\};", src, re.S)]
    assert found, f"{name}[] not found in {path.name}: the code changed shape, fix this test"
    return found


def test_known_is_the_live_commands():
    rows = read_def()
    want = {n for n, r in rows.items() if r["live"]}
    for got in cpp_lists("known"):
        assert got == want, (sorted(got - want), sorted(want - got))


def test_notlive_is_the_rest():
    rows = read_def()
    want = {n for n, r in rows.items() if not r["live"]}
    for got in cpp_lists("notLive"):
        assert got == want, (sorted(got - want), sorted(want - got))


def test_needwin_is_the_window_commands():
    rows = read_def()
    want = {n for n, r in rows.items() if r["win"]}
    lists = cpp_lists("needWin")
    assert len(lists) == 2, "needWin[] is written twice in stage.cpp (run and checkLive)"
    for got in lists:
        assert got == want, (sorted(got - want), sorted(want - got))


if __name__ == "__main__":
    failed = 0
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            try:
                fn()
                print("ok  ", name)
            except AssertionError as e:
                failed += 1
                print("FAIL", name, e)
    sys.exit(1 if failed else 0)
