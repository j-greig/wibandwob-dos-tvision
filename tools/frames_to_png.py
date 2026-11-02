#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow>=10.4.0"]
# ///
"""Render a capture run (POST /capture/start ... /capture/stop) to PNGs, a contact sheet and an mp4.

A run dir holds numbered screen frames (00001-open-wib.txt ...), frames.jsonl (time and beat
of each frame) and events.jsonl. Rendering uses the heartbeat renderer (Unifont, mono,
fixed advance and line pitch), every frame padded to the same grid so the video never jumps.

    uv run tools/frames_to_png.py <run_dir> --out <dir> [--all] [--beats] [--contact] [--mp4 NAME]
      --beats    one PNG per beat: the last frame before the next POST /events/mark
      --all      a PNG for every frame
      --contact  contact sheet of the beat PNGs (or 12 evenly spaced frames when there are no marks)
      --mp4      real-time mp4 from frames.jsonl timings (holds capped at --max-hold seconds)
Env RENDER_ASCII overrides the renderer path.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unicodedata
from pathlib import Path

from PIL import Image

RENDERER = Path(os.environ.get(
    "RENDER_ASCII",
    Path.home() / "Repos/wibandwob-heartbeat/.claude/skills/tweet/scripts/render_ascii.py"))


def load_renderer():
    spec = importlib.util.spec_from_file_location("render_ascii", RENDERER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def cells(s: str) -> int:
    return sum(2 if unicodedata.east_asian_width(c) in ("W", "F") else 1 for c in s)


def normalise(text: str, cols: int, rows: int) -> str:
    lines = text.split("\n")[:rows]
    lines += [""] * (rows - len(lines))
    out = []
    for l in lines:
        w = cells(l)
        out.append(l + " " * max(0, cols - w))
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("run_dir", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--beats", action="store_true")
    ap.add_argument("--contact", action="store_true")
    ap.add_argument("--mp4", default="")
    ap.add_argument("--font-size", type=int, default=16)
    ap.add_argument("--fps", type=int, default=24)
    ap.add_argument("--max-hold", type=float, default=2.5)
    ap.add_argument("--min-hold", type=float, default=0.0)
    a = ap.parse_args()

    run = a.run_dir
    meta = []
    fj = run / "frames.jsonl"
    if fj.exists():
        meta = [json.loads(l) for l in fj.read_text().splitlines() if l.strip()]
    else:
        meta = [{"frame": p.name, "t": i / a.fps, "beat": ""} for i, p in enumerate(sorted(run.glob("[0-9]*.txt")))]
    meta = [m for m in meta if (run / m["frame"]).exists()]
    if not meta:
        print("no frames", file=sys.stderr)
        return 2
    texts = {m["frame"]: (run / m["frame"]).read_text(encoding="utf-8") for m in meta}
    cols = max(cells(l) for t in texts.values() for l in t.split("\n"))
    rows = max(len(t.split("\n")) for t in texts.values())
    ra = load_renderer()
    a.out.mkdir(parents=True, exist_ok=True)
    tmp = Path(tempfile.mkdtemp(prefix="wwdos-frames-"))

    def render(name: str, dest: Path) -> Path:
        ra.render_to_png(normalise(texts[name], cols, rows), dest, font_size=a.font_size,
                         padding=a.font_size, scale=1)
        return dest

    written = []
    beats = []
    if a.beats or a.contact:
        order = []
        for m in meta:
            if m.get("beat") and (not order or order[-1][0] != m["beat"]):
                order.append([m["beat"], m["frame"]])
            elif order and m.get("beat") == order[-1][0]:
                order[-1][1] = m["frame"]
        if not order:
            step = max(1, len(meta) // 12)
            order = [[f"f{i:02d}", meta[i]["frame"]] for i in range(0, len(meta), step)][:12]
        for i, (label, frame) in enumerate(order, 1):
            dest = (a.out if a.beats else tmp) / f"beat-{i}-{label}.png"
            beats.append(render(frame, dest))
        if a.beats:
            written += beats
    if a.all:
        for m in meta:
            written.append(render(m["frame"], a.out / m["frame"].replace(".txt", ".png")))
    if a.contact and beats:
        ims = [Image.open(p) for p in beats]
        tw = 640
        th = int(ims[0].height * tw / ims[0].width)
        ncol = 2 if len(ims) <= 6 else 3
        nrow = (len(ims) + ncol - 1) // ncol
        gap = 12
        sheet = Image.new("RGB", (ncol * tw + (ncol + 1) * gap, nrow * th + (nrow + 1) * gap), (0, 0, 0))
        for i, im in enumerate(ims):
            r, c = divmod(i, ncol)
            sheet.paste(im.convert("RGB").resize((tw, th), Image.LANCZOS),
                        (gap + c * (tw + gap), gap + r * (th + gap)))
        dest = a.out / "contact.png"
        sheet.save(dest)
        written.append(dest)
    if a.mp4:
        fdir = tmp / "mp4"
        fdir.mkdir()
        lst = []
        for i, m in enumerate(meta):
            p = render(m["frame"], fdir / f"{i:05d}.png")
            nxt = meta[i + 1]["t"] if i + 1 < len(meta) else m["t"] + 1.5
            hold = min(a.max_hold, max(a.min_hold, 1 / a.fps, nxt - m["t"]))
            lst.append(f"file '{p}'\nduration {hold:.3f}")
        lst.append(f"file '{fdir / f'{len(meta) - 1:05d}.png'}'")
        (fdir / "list.txt").write_text("\n".join(lst) + "\n")
        dest = a.out / a.mp4
        cmd = ["ffmpeg", "-nostdin", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0",
               "-i", str(fdir / "list.txt"), "-vf", f"fps={a.fps},format=yuv420p,pad=ceil(iw/2)*2:ceil(ih/2)*2",
               "-c:v", "libx264", "-crf", "26", "-preset", "medium", "-movflags", "+faststart", str(dest)]
        subprocess.run(cmd, check=True, stdin=subprocess.DEVNULL)
        written.append(dest)
    shutil.rmtree(tmp, ignore_errors=True)
    for p in written:
        print(p)
    print(f"{len(meta)} frames, grid {cols}x{rows}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
