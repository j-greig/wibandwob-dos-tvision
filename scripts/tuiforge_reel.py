#!/usr/bin/env python3
"""tuiforge_reel.py — staggered TUIFORGE exhibition + mp4 reel.

Spawns tuiforge render windows one by one at composed positions while
grabbing frames of the Ghostty window, then encodes an mp4 with ffmpeg.
No asciinema: wwdos runs in a real Ghostty surface with RGB skins that
cast files would mangle — we record actual pixels.

Usage:
  python3 scripts/tuiforge_reel.py                 # full reel (~36 renders)
  python3 scripts/tuiforge_reel.py --test          # short 6-render smoke
  python3 scripts/tuiforge_reel.py --out /tmp/x.mp4 --interval 1.2

Requires: wwdos + api server running, ffmpeg, Ghostty frontmost at 60,40
2400x1360 (relaunch script default).
"""
import argparse
import json
import subprocess
import sys
import time
import urllib.request

API = "http://localhost:8089"

# The exhibition set: recent custom wibwob corpus. Portraits are 80x50,
# landscapes 80x25 — positions composed for the 299x77 desktop as a salon
# wall (overlap deliberate, later windows sit on top).
PORTRAITS = [
    "tall-wib-painterly-portrait",
    "tall-wob-painterly-portrait",
    "tall-scramble-gallery-portrait",
    "tall-wibwobworld-map-painterly-portrait",
    "tall-umwelt-tick-portrait",
    "tall-wibwob-meet-substrate",
    "tall-wib-fossil-field-plate",
    "tall-wrong-cell-painterly-portrait",
    "tall-warm-faced-loaf-painterly-portrait",
    "tall-zombie-wibwob-loadscreen",
    "tall-wibwob-horror-terminal-01-breathing-door",
    "tall-wibwob-horror-terminal-03-underfloor-mouth",
    "weird-01-chime-mouth",
    "weird-02-scramble-ate-moon",
    "weird-03-lost-sock-archive",
    "zzt-suite-01-worm-god-arena",
    "zzt-suite-02-symbient-select",
    "zzt-suite-07-wib-dream-cutscene",
    "zzt-hyperspace-warpgate",
    "taxonomy-specimen",
    "tufte-01-wibwob-sparkline-dashboard",
    "the-missing-9-painter-b",
    "the-missing-9-kindle-B-1-hexdump-altar",
    "the-missing-9-kindle-A-3-osiris-effigy-diptych",
    "the-missing-9-collage-03-cga-riot",
    "tdr-pollock-mycelium-scream-v3",
]
LANDSCAPES = [
    "everywhen-chaos-poster",
    "everywhen-disco-hoch",
    "everywhen-stark-poster",
    "sonnet-everywhen-rainbow-poster",
    "cga-everywhen-redo",
    "figlet-zzt-wob-explains-everywhen",
    "kevart/cat3d",
    "sonnet-everywhen-melted-trippy",
    "anim-scramble-portrait",
    "ascii-hyperspace-everywhen-v2",
]

DESK_W, DESK_H = 299, 77


def positions():
    """Yield staggered salon-wall rects: portraits brick-laid two deep,
    landscapes filling the bottom band and gaps."""
    seq = []
    # Portrait wall: two staggered rows sweeping left to right.
    xs_a = list(range(0, DESK_W - 82 + 1, 36))
    for i, x in enumerate(xs_a):
        seq.append((x, 0 if i % 2 == 0 else 13, 82, 52))
    xs_b = list(range(18, DESK_W - 82 + 1, 36))
    for i, x in enumerate(xs_b):
        seq.append((x, 25 if i % 2 == 0 else 18, 82, 52))
    # Landscape band along the bottom.
    for i, x in enumerate(range(0, DESK_W - 82 + 1, 44)):
        seq.append((x, 50 if i % 2 == 0 else 44, 82, 27))
    # Fill: centre-ish accents.
    seq += [(108, 12, 82, 52), (36, 6, 82, 27), (180, 40, 82, 27)]
    # Layer two: a second staggered sweep so the wall keeps thickening —
    # later windows overlap earlier ones (salon hang, newest on top).
    for i, x in enumerate(range(9, DESK_W - 82 + 1, 42)):
        seq.append((x, 4 + (i % 3) * 9, 82, 52))
    for i, x in enumerate(range(30, DESK_W - 82 + 1, 52)):
        seq.append((x, 46 if i % 2 == 0 else 34, 82, 27))
    for i, x in enumerate(range(60, DESK_W - 82 + 1, 64)):
        seq.append((x, 16 + (i % 2) * 14, 82, 52))
    return seq


def spawn(path, rect):
    body = json.dumps({
        "type": "tuiforge",
        "rect": {"x": rect[0], "y": rect[1], "w": rect[2], "h": rect[3]},
        "props": {"path": path},
    }).encode()
    req = urllib.request.Request(API + "/windows", data=body,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.load(r)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--test", action="store_true", help="6 renders, quick")
    ap.add_argument("--out", default="/tmp/tuiforge_reel.mp4")
    ap.add_argument("--interval", type=float, default=1.2,
                    help="seconds between window spawns")
    ap.add_argument("--fps", type=int, default=4, help="capture rate")
    args = ap.parse_args()

    # Capture by CGWindowID, NEVER by region: region capture records
    # whatever occupies those pixels — if Ghostty loses frontmost mid-reel
    # the video swallows the user's other apps and private windows
    # (happened 2026-08-13: a reel captured a private chat; binned).
    # -l <id> can only ever see the Ghostty window itself.
    import Quartz  # pyobjc, present on the studio machine
    wins = Quartz.CGWindowListCopyWindowInfo(
        Quartz.kCGWindowListOptionOnScreenOnly, Quartz.kCGNullWindowID)
    ghost = [w.get("kCGWindowNumber") for w in wins
             if w.get("kCGWindowOwnerName") == "Ghostty"
             and w.get("kCGWindowName") == "👻"]
    if not ghost:
        sys.exit("no 👻 Ghostty window found — is wwdos running?")
    if len(ghost) > 1:
        sys.exit(f"multiple 👻 windows {ghost} — sweep strays first "
                 "(scripts/relaunch_wwdos.sh does this)")
    win_id = ghost[0]
    print(f"capturing Ghostty window id {win_id}")

    renders = ([(p, True) for p in PORTRAITS] +
               [(l, False) for l in LANDSCAPES])
    pos = positions()
    n = 6 if args.test else min(len(renders), len(pos))
    plan = list(zip([r for r, _ in renders][:n], pos[:n]))

    subprocess.run(["osascript", "-e",
                    'tell application "Ghostty" to activate'], check=False)
    time.sleep(1)

    frames_dir = "/tmp/tuiforge_frames"
    subprocess.run(["rm", "-rf", frames_dir], check=False)
    subprocess.run(["mkdir", "-p", frames_dir], check=True)

    total = n * args.interval + 3.0   # linger on the full wall at the end
    frame_period = 1.0 / args.fps
    t0 = time.time()
    next_spawn = 0.0
    spawned = 0
    frame = 0
    while time.time() - t0 < total:
        now = time.time() - t0
        if spawned < n and now >= next_spawn:
            path, rect = plan[spawned]
            try:
                spawn(path, rect)
                print(f"[{now:5.1f}s] + {path} @ {rect}")
            except Exception as e:
                print(f"[{now:5.1f}s] ! {path}: {e}", file=sys.stderr)
            spawned += 1
            next_spawn += args.interval
        subprocess.run(["screencapture", "-x", "-l", str(win_id),
                        f"{frames_dir}/f{frame:05d}.png"], check=False)
        frame += 1
        # sleep whatever remains of the frame period
        elapsed = (time.time() - t0) - now
        if elapsed < frame_period:
            time.sleep(frame_period - elapsed)

    # Encode at the ACHIEVED capture rate (screencapture costs ~0.5s a
    # frame, so requested fps overstates reality and the video plays fast).
    # 4K/lanczos/crf16: retina frames are ~4936px wide and dense ASCII text
    # dies under double-downscaling — encode tall (2160) so the only
    # downscale is the platform's own. A 1920 cut of a 299-col terminal
    # gives ~6px per glyph and reads as fuzz (learned 2026-08-13; the 1920
    # tweet was deleted and reposted in 4K).
    real_fps = max(1.0, frame / (time.time() - t0))
    print(f"{frame} frames captured ({real_fps:.2f} fps achieved); encoding …")
    subprocess.run([
        "ffmpeg", "-y", "-framerate", f"{real_fps:.3f}",
        "-i", f"{frames_dir}/f%05d.png",
        "-c:v", "libx264", "-preset", "slow", "-crf", "16",
        "-pix_fmt", "yuv420p",
        "-vf", "scale=-2:2160:flags=lanczos,setsar=1", args.out,
    ], check=True, capture_output=True)
    print(f"reel: {args.out}")


if __name__ == "__main__":
    main()
