#!/usr/bin/env python3
"""window_ballet.py — choreographed window animation (the Flash timeline, but
it's the desktop itself dancing).

Spawns a cast of windows, then drives their x/y through eased keyframe
phases via move_window at a gentle command rate, while capturing frames of
the Ghostty window (by CGWindowID — never region) for an mp4.

Phases: FLY-IN (edges -> ring) · ORBIT (ring rotates, radius breathes) ·
HEARTBEAT (collapse/bounce x2) · SCATTER (fling out, one survivor centres).

Usage: python3 scripts/window_ballet.py [--out output/window_ballet.mp4]
       [--fps 3] [--rate 8]   # rate = move commands/sec budget
"""
import argparse, json, math, subprocess, sys, time, urllib.request

API = "http://localhost:8089"
DESK_W, DESK_H = 299, 77
CX, CY = DESK_W // 2, DESK_H // 2 - 2


def cmd(command, args=None, timeout=15):
    body = json.dumps({"command": command, "args": args or {}}).encode()
    req = urllib.request.Request(API + "/menu/command", data=body,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)


def create(wtype, rect, props=None):
    body = json.dumps({"type": wtype, "rect": rect, "props": props or {}}).encode()
    req = urllib.request.Request(API + "/windows", data=body,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.load(r)["id"]


def ease(t):
    """cubic in-out on 0..1"""
    t = max(0.0, min(1.0, t))
    return 4*t*t*t if t < 0.5 else 1 - pow(-2*t + 2, 3) / 2


def ghost_window_id():
    import Quartz
    wins = Quartz.CGWindowListCopyWindowInfo(
        Quartz.kCGWindowListOptionOnScreenOnly, Quartz.kCGNullWindowID)
    ids = [w.get("kCGWindowNumber") for w in wins
           if w.get("kCGWindowOwnerName") == "Ghostty"
           and w.get("kCGWindowName") == "👻"]
    if len(ids) != 1:
        sys.exit(f"need exactly one 👻 window, found {ids}")
    return ids[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="output/window_ballet.mp4")
    ap.add_argument("--fps", type=int, default=3)
    ap.add_argument("--rate", type=float, default=8.0)
    args = ap.parse_args()

    win_id = ghost_window_id()
    cmd("close_all"); time.sleep(0.5)
    cmd("set_skin", {"skin": "midnight"})
    cmd("screensaver", {"action": "off"})

    # ── the cast ──────────────────────────────────────────────
    # (type, w, h, props) — the full company: hero portraits (tuiforge
    # fit-downsamples whole artworks into flying frames), a primer, and
    # the generative corps de ballet.
    troupe_def = [
        ("tuiforge", 28, 17, {"path": "tall-wib-painterly-portrait"}),
        ("tuiforge", 28, 17, {"path": "tall-wob-painterly-portrait"}),
        ("tuiforge", 28, 17, {"path": "tall-scramble-gallery-portrait"}),
        ("tuiforge", 26, 10, {"path": "weird-02-scramble-ate-moon"}),
        ("tuiforge", 30, 11, {"path": "everywhen-chaos-poster"}),
        ("frame_player", 30, 12,
         {"path": "modules/wibwob-primers/primers/symbient-status.txt"}),
        ("cube", 14, 6, None), ("cube", 14, 6, None),
        ("torus", 14, 6, None),
        ("orbit", 16, 7, None), ("mycelium", 16, 7, None),
        ("shader", 24, 10, None),
    ]
    # off-stage starts: distributed around the edges
    cast = []
    for i, (t, w, h, props) in enumerate(troupe_def):
        edge = i % 4
        if edge == 0:   sx, sy = -w + 1, (i * 13) % DESK_H
        elif edge == 1: sx, sy = DESK_W - 1, (i * 17) % DESK_H
        elif edge == 2: sx, sy = (i * 37) % DESK_W, -h + 1
        else:           sx, sy = (i * 29) % DESK_W, DESK_H - 2
        wid = create(t, {"x": sx, "y": sy, "w": w, "h": h}, props)
        cast.append({"id": wid, "w": w, "h": h, "sx": sx, "sy": sy})
    # centrepiece: a tuiforge still, born centre-stage, mostly still.
    # Big enough for the art to READ (with the view's fit-downsample as
    # belt-and-braces; a 40x14 cat was once unrecognisable mush).
    centre = create("tuiforge", {"x": CX - 31, "y": CY - 11, "w": 62, "h": 24},
                    {"path": "kevart/cat3d"})

    n = len(cast)
    ring_r_x, ring_r_y = 90, 26     # ellipse (cells are 1:2)

    def ring_pos(i, phase, rscale=1.0):
        a = phase + i * 2 * math.pi / n
        x = CX + ring_r_x * rscale * math.cos(a)
        y = CY + ring_r_y * rscale * math.sin(a)
        return int(x), int(y)

    # ── frame capture (background) ────────────────────────────
    frames_dir = "/tmp/ballet_frames"
    subprocess.run(["rm", "-rf", frames_dir]); subprocess.run(["mkdir", "-p", frames_dir])
    subprocess.run(["osascript", "-e", 'tell application "Ghostty" to activate'])
    time.sleep(0.8)
    cap = subprocess.Popen(["bash", "-c",
        f'i=0; while true; do screencapture -x -l {win_id} {frames_dir}/f$(printf %05d $i).png; i=$((i+1)); sleep {1.0/args.fps}; done'])

    tick_dt = 1.0 / args.rate
    t0 = time.time()

    def move(wid, x, y):
        try:
            cmd("move_window", {"id": wid, "x": str(int(x)), "y": str(int(y))}, timeout=5)
        except Exception:
            pass   # one dropped beat must not stop the dance

    try:
        # PHASE 1 — FLY-IN (8s): edges -> ring, staggered entrances
        P1 = 8.0
        while (el := time.time() - t0) < P1:
            for i, c in enumerate(cast):
                lt = ease((el - i * 0.4) / (P1 - i * 0.4 or 1))
                tx, ty = ring_pos(i, 0.0)
                move(c["id"], c["sx"] + (tx - c["sx"]) * lt,
                              c["sy"] + (ty - c["sy"]) * lt)
                time.sleep(tick_dt / n)
        # PHASE 2 — ORBIT (18s): ring rotates, radius breathes
        P2 = 18.0; t1 = time.time()
        while (el := time.time() - t1) < P2:
            phase = el * 2 * math.pi / 12.0            # one rev / 12s
            rscale = 1.0 + 0.18 * math.sin(el * 2 * math.pi / 6.0)
            for i, c in enumerate(cast):
                tx, ty = ring_pos(i, phase, rscale)
                move(c["id"], tx - c["w"] // 2, ty - c["h"] // 2)
                time.sleep(tick_dt / n)
        # PHASE 3 — HEARTBEAT (9s): collapse to centre and bounce, twice
        P3 = 9.0; t2 = time.time()
        while (el := time.time() - t2) < P3:
            beat = abs(math.sin(el * 2 * math.pi / 4.5))   # 2 beats
            rscale = 0.15 + 0.85 * beat
            for i, c in enumerate(cast):
                tx, ty = ring_pos(i, 0.0, rscale)
                move(c["id"], tx - c["w"] // 2, ty - c["h"] // 2)
                time.sleep(tick_dt / n)
        # PHASE 4 — SCATTER (6s): fling out, cat remains
        P4 = 6.0; t3 = time.time()
        while (el := time.time() - t3) < P4:
            lt = ease(el / P4)
            for i, c in enumerate(cast):
                rx, ry = ring_pos(i, 0.0, 0.15)
                fx, fy = ring_pos(i, 0.0, 2.6)          # past the edges
                move(c["id"], rx + (fx - rx) * lt - c["w"] // 2,
                              ry + (fy - ry) * lt - c["h"] // 2)
                time.sleep(tick_dt / n)
        cmd("raise_window", {"id": centre})
        time.sleep(2.5)                                  # linger on the cat
    finally:
        cap.terminate()

    # ── encode (4K lanczos crf16 — the tuiforge_reel lesson) ──
    nframes = int(subprocess.run(["bash", "-c", f"ls {frames_dir} | wc -l"],
                                 capture_output=True, text=True).stdout)
    real_fps = max(1.0, nframes / (time.time() - t0))
    print(f"{nframes} frames ({real_fps:.2f} fps); encoding …")
    subprocess.run(["ffmpeg", "-y", "-framerate", f"{real_fps:.3f}",
                    "-i", f"{frames_dir}/f%05d.png",
                    "-c:v", "libx264", "-preset", "slow", "-crf", "16",
                    "-pix_fmt", "yuv420p",
                    "-vf", "scale=-2:2160:flags=lanczos,setsar=1", args.out],
                   check=True, capture_output=True)
    print(f"ballet: {args.out}")


if __name__ == "__main__":
    main()
