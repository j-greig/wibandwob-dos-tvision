#!/usr/bin/env python3
"""
Contour map generator using Unicode box-drawing characters.
Marching squares on a heightmap of gaussian hills.

Characters used:
  ╭ ╮ ╰ ╯ │ ─

Run: python3 contour_map.py
"""

import math
import random
import sys
import os

# ── character lookup ──────────────────────────────────────
# Marching squares: 4 corners (TL TR BR BL) as bits 3..0
# Each case tells us which cell edges the contour crosses,
# and that maps to a box-drawing character.
#
#   ╭ connects right + bottom    ╮ connects left + bottom
#   ╰ connects right + top       ╯ connects left + top
#   ─ connects left + right      │ connects top + bottom

MS = [
    ' ',  # 0000  no contour
    '╮',  # 0001  left + bottom
    '╭',  # 0010  right + bottom
    '─',  # 0011  left + right
    '╰',  # 0100  right + top
    '╮',  # 0101  saddle (approx)
    '│',  # 0110  top + bottom
    '╯',  # 0111  left + top
    '╯',  # 1000  left + top
    '│',  # 1001  top + bottom
    '╭',  # 1010  saddle (approx)
    '╰',  # 1011  right + top
    '─',  # 1100  left + right
    '╭',  # 1101  right + bottom
    '╮',  # 1110  left + bottom
    ' ',  # 1111  no contour
]


def make_heightmap(w, h, hills):
    """Sum of gaussian hills. Returns 2D list of floats in [0,1]."""
    grid = [[0.0] * w for _ in range(h)]
    for cx, cy, radius, peak in hills:
        r2 = radius * radius
        # only bother within 3*radius
        y0 = max(0, int(cy - 3 * radius))
        y1 = min(h, int(cy + 3 * radius) + 1)
        x0 = max(0, int(cx - 3 * radius))
        x1 = min(w, int(cx + 3 * radius) + 1)
        for y in range(y0, y1):
            for x in range(x0, x1):
                d2 = (x - cx) ** 2 + (y - cy) ** 2
                grid[y][x] += peak * math.exp(-d2 / (2 * r2))
    # normalise to [0, 1]
    flat = [v for row in grid for v in row]
    lo, hi = min(flat), max(flat)
    if hi - lo < 1e-9:
        return grid
    for y in range(h):
        for x in range(w):
            grid[y][x] = (grid[y][x] - lo) / (hi - lo)
    return grid


def march(grid, h, w, threshold):
    """Marching squares at one threshold. Returns 2D char array (h-1 x w-1)."""
    rows = []
    for y in range(h - 1):
        row = []
        for x in range(w - 1):
            tl = 1 if grid[y][x] >= threshold else 0
            tr = 1 if grid[y][x + 1] >= threshold else 0
            br = 1 if grid[y + 1][x + 1] >= threshold else 0
            bl = 1 if grid[y + 1][x] >= threshold else 0
            idx = (tl << 3) | (tr << 2) | (br << 1) | bl
            row.append(MS[idx])
        rows.append(row)
    return rows


def composite(layers):
    """Overlay multiple contour layers. Later layers overwrite spaces only kept from earlier."""
    h = len(layers[0])
    w = len(layers[0][0])
    canvas = [[' '] * w for _ in range(h)]
    for layer in layers:
        for y in range(h):
            for x in range(w):
                if layer[y][x] != ' ':
                    canvas[y][x] = layer[y][x]
    return canvas


def random_hills(w, h, n_hills, seed=None):
    """Generate random hill parameters."""
    rng = random.Random(seed)
    hills = []
    for _ in range(n_hills):
        cx = rng.uniform(w * 0.1, w * 0.9)
        cy = rng.uniform(h * 0.1, h * 0.9)
        radius = rng.uniform(min(w, h) * 0.06, min(w, h) * 0.25)
        peak = rng.uniform(0.4, 1.0)
        hills.append((cx, cy, radius, peak))
    return hills


def clustered_hills(w, h, seed=None):
    """Some solo hills, some clumped pairs/triples — more natural."""
    rng = random.Random(seed)
    hills = []

    n_clusters = rng.randint(3, 7)
    for _ in range(n_clusters):
        cx = rng.uniform(w * 0.15, w * 0.85)
        cy = rng.uniform(h * 0.15, h * 0.85)
        cluster_size = rng.choices([1, 2, 3], weights=[3, 4, 2])[0]
        for j in range(cluster_size):
            ox = rng.gauss(0, min(w, h) * 0.06) if j > 0 else 0
            oy = rng.gauss(0, min(w, h) * 0.06) if j > 0 else 0
            radius = rng.uniform(min(w, h) * 0.05, min(w, h) * 0.18)
            peak = rng.uniform(0.3, 1.0)
            hills.append((cx + ox, cy + oy, radius, peak))

    # a few small lone bumps for texture
    for _ in range(rng.randint(2, 5)):
        hills.append((
            rng.uniform(0, w),
            rng.uniform(0, h),
            rng.uniform(min(w, h) * 0.03, min(w, h) * 0.07),
            rng.uniform(0.2, 0.5),
        ))
    return hills


def render_map(w=100, h=45, n_levels=6, seed=None, style='clustered'):
    """Generate and print a contour map."""
    if style == 'clustered':
        hills = clustered_hills(w, h, seed=seed)
    else:
        hills = random_hills(w, h, 8, seed=seed)

    grid = make_heightmap(w, h, hills)

    # pick contour thresholds evenly spaced
    thresholds = [i / (n_levels + 1) for i in range(1, n_levels + 1)]

    layers = [march(grid, h, w, t) for t in thresholds]
    canvas = composite(layers)

    out = []
    for row in canvas:
        out.append(''.join(row))
    return '\n'.join(out)


# ── main ──────────────────────────────────────────────────

if __name__ == '__main__':
    # try to fill terminal width
    try:
        tw = os.get_terminal_size().columns
        th = os.get_terminal_size().lines - 4
    except OSError:
        tw, th = 100, 40

    seed = int(sys.argv[1]) if len(sys.argv) > 1 else None

    print()
    print(f"  CONTOUR MAP  seed={seed or 'random'}  {tw}x{th}")
    print(f"  {'─' * (tw - 4)}")
    print(render_map(w=tw, h=th, n_levels=6, seed=seed, style='clustered'))
    print()

    # show a second map with different seed for comparison
    seed2 = (seed or random.randint(0, 9999)) + 1
    print(f"  CONTOUR MAP  seed={seed2}  {tw}x{th}")
    print(f"  {'─' * (tw - 4)}")
    print(render_map(w=tw, h=th, n_levels=6, seed=seed2, style='clustered'))
    print()
