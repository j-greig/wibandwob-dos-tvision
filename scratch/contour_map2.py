#!/usr/bin/env python3
"""
Contour map generator v2 — cleaner compositing, varied terrain styles.
Uses only: ╭ ╮ ╰ ╯ │ ─

Run:  python3 contour_map2.py [seed]
"""

import math
import random
import sys
import os

# ── marching squares lookup ───────────────────────────────
# Corners: TL(bit3) TR(bit2) BR(bit1) BL(bit0)
# Character connects the two cell edges the contour crosses.
#   ╭ = right+bottom   ╮ = left+bottom
#   ╰ = right+top      ╯ = left+top
#   ─ = left+right     │ = top+bottom

MS = ' ╮╭─╰╮│╯╯│╭╰─╭╮ '
# idx: 0123456789ABCDEF


def heightmap(w, h, hills):
    """Sum of gaussians, normalised to [0,1]."""
    g = [[0.0] * w for _ in range(h)]
    for cx, cy, r, pk in hills:
        r2 = 2.0 * r * r
        iy0, iy1 = max(0, int(cy - 3*r)), min(h, int(cy + 3*r) + 1)
        ix0, ix1 = max(0, int(cx - 3*r)), min(w, int(cx + 3*r) + 1)
        for y in range(iy0, iy1):
            dy2 = (y - cy) ** 2
            for x in range(ix0, ix1):
                g[y][x] += pk * math.exp(-((x-cx)**2 + dy2) / r2)
    lo = min(v for row in g for v in row)
    hi = max(v for row in g for v in row)
    if hi - lo < 1e-9:
        return g
    s = 1.0 / (hi - lo)
    for y in range(h):
        for x in range(w):
            g[y][x] = (g[y][x] - lo) * s
    return g


def march(g, h, w, thr):
    """Single-threshold marching squares → char grid (h-1 × w-1)."""
    rows = []
    for y in range(h - 1):
        row = []
        for x in range(w - 1):
            idx = 0
            if g[y][x]     >= thr: idx |= 8
            if g[y][x+1]   >= thr: idx |= 4
            if g[y+1][x+1] >= thr: idx |= 2
            if g[y+1][x]   >= thr: idx |= 1
            row.append(MS[idx])
        rows.append(row)
    return rows


def composite(layers):
    """Stack contour layers. Inner (later) contours overwrite outer ones."""
    rh = len(layers[0])
    rw = len(layers[0][0])
    canvas = [[' '] * rw for _ in range(rh)]
    for layer in layers:
        for y in range(rh):
            for x in range(rw):
                ch = layer[y][x]
                if ch != ' ':
                    canvas[y][x] = ch
    return canvas


# ── terrain generators ────────────────────────────────────

def terrain_gentle_ridges(w, h, rng):
    """Long ridges with a few peaks — like the Downs."""
    hills = []
    for _ in range(3):
        cx = rng.uniform(w*0.15, w*0.85)
        cy = rng.uniform(h*0.2, h*0.8)
        # elongated: wide radius, moderate peak
        rx = rng.uniform(w*0.15, w*0.35)
        hills.append((cx, cy, rx, rng.uniform(0.6, 1.0)))
        # smaller bumps along the ridge
        for j in range(rng.randint(2, 4)):
            ox = rng.gauss(0, rx * 0.4)
            oy = rng.gauss(0, min(w,h) * 0.03)
            hills.append((cx+ox, cy+oy, rng.uniform(w*0.03, w*0.08), rng.uniform(0.3, 0.7)))
    return hills


def terrain_volcanic_cluster(w, h, rng):
    """Tight steep peaks, some solo, some clumped."""
    hills = []
    # 2-3 tight clusters
    for _ in range(rng.randint(2, 4)):
        cx = rng.uniform(w*0.1, w*0.9)
        cy = rng.uniform(h*0.1, h*0.9)
        n = rng.choices([1, 2, 3], weights=[2, 3, 2])[0]
        for j in range(n):
            ox = rng.gauss(0, min(w,h)*0.04) if j else 0
            oy = rng.gauss(0, min(w,h)*0.04) if j else 0
            r = rng.uniform(min(w,h)*0.04, min(w,h)*0.10)
            hills.append((cx+ox, cy+oy, r, rng.uniform(0.5, 1.0)))
    # scattered small bumps
    for _ in range(rng.randint(3, 6)):
        hills.append((
            rng.uniform(0, w), rng.uniform(0, h),
            rng.uniform(min(w,h)*0.02, min(w,h)*0.05),
            rng.uniform(0.15, 0.4),
        ))
    return hills


def terrain_rolling_meadow(w, h, rng):
    """Broad, overlapping low hills — gentle and spacious."""
    hills = []
    for _ in range(rng.randint(5, 9)):
        hills.append((
            rng.uniform(w*0.05, w*0.95),
            rng.uniform(h*0.05, h*0.95),
            rng.uniform(min(w,h)*0.10, min(w,h)*0.25),
            rng.uniform(0.3, 0.8),
        ))
    # a couple of sharper knolls
    for _ in range(rng.randint(1, 3)):
        hills.append((
            rng.uniform(w*0.1, w*0.9),
            rng.uniform(h*0.1, h*0.9),
            rng.uniform(min(w,h)*0.04, min(w,h)*0.08),
            rng.uniform(0.6, 1.0),
        ))
    return hills


def terrain_twin_peaks(w, h, rng):
    """Two big hills merging at the base with saddle between."""
    sep = rng.uniform(w*0.15, w*0.30)
    cx = w * 0.5
    cy = h * 0.5
    r1 = rng.uniform(min(w,h)*0.12, min(w,h)*0.20)
    r2 = rng.uniform(min(w,h)*0.10, min(w,h)*0.18)
    hills = [
        (cx - sep*0.5, cy, r1, 1.0),
        (cx + sep*0.5, cy, r2, rng.uniform(0.7, 0.95)),
    ]
    # foothills
    for _ in range(rng.randint(3, 6)):
        angle = rng.uniform(0, math.tau)
        dist = rng.uniform(sep*0.3, sep*1.5)
        hills.append((
            cx + math.cos(angle)*dist,
            cy + math.sin(angle)*dist,
            rng.uniform(min(w,h)*0.03, min(w,h)*0.07),
            rng.uniform(0.2, 0.5),
        ))
    return hills


TERRAINS = {
    'ridges':  terrain_gentle_ridges,
    'volcanic': terrain_volcanic_cluster,
    'meadow':  terrain_rolling_meadow,
    'twins':   terrain_twin_peaks,
}


def render(w, h, n_levels, seed, style):
    rng = random.Random(seed)
    hills = TERRAINS[style](w, h, rng)
    g = heightmap(w, h, hills)

    thresholds = [(i+1) / (n_levels+1) for i in range(n_levels)]
    layers = [march(g, h, w, t) for t in thresholds]
    canvas = composite(layers)
    return '\n'.join(''.join(row) for row in canvas)


if __name__ == '__main__':
    try:
        tw = os.get_terminal_size().columns
        th = os.get_terminal_size().lines - 3
    except OSError:
        tw, th = 100, 40

    seed = int(sys.argv[1]) if len(sys.argv) > 1 else random.randint(0, 99999)

    # show all four terrain styles
    half_h = max(18, th // 2 - 2)

    for style in ['volcanic', 'twins', 'meadow', 'ridges']:
        label = f" {style.upper()}  seed={seed} "
        pad = tw - len(label) - 2
        print(f"╭{label}{'─' * pad}╮")
        lines = render(tw - 2, half_h, 7, seed, style).split('\n')
        for ln in lines:
            # pad to exact width
            ln = ln[:tw-2].ljust(tw-2)
            print(f"│{ln}│")
        print(f"╰{'─' * (tw - 2)}╯")
        print()
