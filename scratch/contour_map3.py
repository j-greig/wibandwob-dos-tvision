#!/usr/bin/env python3
"""
Contour map generator v3 — gallery of terrain styles.
Uses only: ╭ ╮ ╰ ╯ │ ─

Run:  python3 contour_map3.py [seed]
"""

import math, random, sys, os

MS = ' ╮╭─╰╮│╯╯│╭╰─╭╮ '


def heightmap(w, h, hills):
    g = [[0.0]*w for _ in range(h)]
    for cx, cy, r, pk in hills:
        r2 = 2.0*r*r
        y0, y1 = max(0,int(cy-3*r)), min(h,int(cy+3*r)+1)
        x0, x1 = max(0,int(cx-3*r)), min(w,int(cx+3*r)+1)
        for y in range(y0, y1):
            dy2 = (y-cy)**2
            for x in range(x0, x1):
                g[y][x] += pk * math.exp(-((x-cx)**2+dy2)/r2)
    lo = min(v for row in g for v in row)
    hi = max(v for row in g for v in row)
    if hi-lo < 1e-9: return g
    s = 1.0/(hi-lo)
    for y in range(h):
        for x in range(w):
            g[y][x] = (g[y][x]-lo)*s
    return g


def march(g, h, w, thr):
    rows = []
    for y in range(h-1):
        row = []
        for x in range(w-1):
            idx = 0
            if g[y][x]     >= thr: idx |= 8
            if g[y][x+1]   >= thr: idx |= 4
            if g[y+1][x+1] >= thr: idx |= 2
            if g[y+1][x]   >= thr: idx |= 1
            row.append(MS[idx])
        rows.append(row)
    return rows


def composite(layers):
    rh, rw = len(layers[0]), len(layers[0][0])
    canvas = [[' ']*rw for _ in range(rh)]
    for layer in layers:
        for y in range(rh):
            for x in range(rw):
                if layer[y][x] != ' ':
                    canvas[y][x] = layer[y][x]
    return canvas


# ── terrain generators ────────────────────────────────────

def t_archipelago(w, h, rng):
    """Scattered island peaks rising from a flat sea."""
    hills = []
    for _ in range(rng.randint(6, 12)):
        hills.append((
            rng.uniform(w*0.05, w*0.95),
            rng.uniform(h*0.05, h*0.95),
            rng.uniform(min(w,h)*0.04, min(w,h)*0.12),
            rng.uniform(0.3, 1.0),
        ))
    return hills


def t_saddle_pass(w, h, rng):
    """Two big hills with a pass/saddle between + foothills."""
    gap = rng.uniform(w*0.20, w*0.35)
    cx, cy = w*0.5, h*0.5
    r = min(w,h)*0.18
    hills = [
        (cx - gap*0.5, cy - h*0.05, r, 1.0),
        (cx + gap*0.5, cy + h*0.05, r*0.9, 0.85),
    ]
    # foothills
    for _ in range(rng.randint(2, 5)):
        a = rng.uniform(0, math.tau)
        d = rng.uniform(r*0.8, r*2.0)
        hills.append((cx+math.cos(a)*d, cy+math.sin(a)*d,
                       rng.uniform(min(w,h)*0.03, min(w,h)*0.07), rng.uniform(0.15, 0.4)))
    return hills


def t_ridge_valley(w, h, rng):
    """A long ridge running across with a valley beside it."""
    hills = []
    # main ridge: chain of overlapping bumps
    ry = h * rng.uniform(0.3, 0.5)
    for i in range(8):
        x = w * (i+0.5) / 8
        hills.append((x, ry + rng.gauss(0, h*0.03),
                       rng.uniform(min(w,h)*0.08, min(w,h)*0.14),
                       rng.uniform(0.6, 1.0)))
    # secondary ridge lower
    ry2 = h * rng.uniform(0.65, 0.8)
    for i in range(5):
        x = w * (i+0.5) / 5
        hills.append((x, ry2 + rng.gauss(0, h*0.02),
                       rng.uniform(min(w,h)*0.05, min(w,h)*0.10),
                       rng.uniform(0.3, 0.6)))
    return hills


def t_caldera(w, h, rng):
    """A ring of peaks around a depression — like a crater rim."""
    cx, cy = w*0.5, h*0.5
    rim_r = min(w,h) * rng.uniform(0.20, 0.30)
    n = rng.randint(6, 10)
    hills = []
    for i in range(n):
        a = math.tau * i / n + rng.gauss(0, 0.2)
        px = cx + math.cos(a) * rim_r
        py = cy + math.sin(a) * rim_r
        hills.append((px, py,
                       rng.uniform(min(w,h)*0.06, min(w,h)*0.12),
                       rng.uniform(0.6, 1.0)))
    # small central bump (lava dome)
    hills.append((cx + rng.gauss(0, 2), cy + rng.gauss(0, 2),
                   min(w,h)*0.04, 0.5))
    return hills


def t_lone_peak(w, h, rng):
    """One dominant mountain with radiating foothills."""
    cx = w * rng.uniform(0.35, 0.65)
    cy = h * rng.uniform(0.35, 0.65)
    r = min(w,h) * 0.20
    hills = [(cx, cy, r, 1.0)]
    for _ in range(rng.randint(4, 8)):
        a = rng.uniform(0, math.tau)
        d = rng.uniform(r*0.6, r*2.5)
        hills.append((cx+math.cos(a)*d, cy+math.sin(a)*d,
                       rng.uniform(min(w,h)*0.03, min(w,h)*0.08),
                       rng.uniform(0.15, 0.45)))
    return hills


TERRAINS = {
    'archipelago':  t_archipelago,
    'saddle pass':  t_saddle_pass,
    'ridge valley': t_ridge_valley,
    'caldera':      t_caldera,
    'lone peak':    t_lone_peak,
}


def render(w, h, n_levels, seed, style):
    rng = random.Random(seed)
    hills = TERRAINS[style](w, h, rng)
    g = heightmap(w, h, hills)
    thresholds = [(i+1)/(n_levels+1) for i in range(n_levels)]
    layers = [march(g, h, w, t) for t in thresholds]
    canvas = composite(layers)
    return '\n'.join(''.join(row) for row in canvas)


def boxed(title, content, tw):
    """Wrap content in a box."""
    lines = content.split('\n')
    out = []
    label = f" {title} "
    pad = tw - len(label) - 2
    out.append(f"╭{label}{'─'*pad}╮")
    for ln in lines:
        ln = ln[:tw-2].ljust(tw-2)
        out.append(f"│{ln}│")
    out.append(f"╰{'─'*(tw-2)}╯")
    return '\n'.join(out)


if __name__ == '__main__':
    try:
        tw = os.get_terminal_size().columns
    except OSError:
        tw = 100

    seed = int(sys.argv[1]) if len(sys.argv) > 1 else random.randint(0, 99999)
    panel_h = 22
    n_levels = 5  # fewer levels = cleaner lines

    for style in TERRAINS:
        print(boxed(f"{style.upper()}  seed={seed}", render(tw-2, panel_h, n_levels, seed, style), tw))
        print()
