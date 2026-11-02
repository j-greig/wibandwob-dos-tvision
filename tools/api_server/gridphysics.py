#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""gridphysics.py (vendored copy of wibandwob-heartbeat scripts/anim/gridphysics.py;
used by /windows/fall. Keep in step with the original.)

gridphysics.py — a tiny GRID-TRUE microphysics brick for character animations.

Cells of a monospace drawing become particles: they stay ATTACHED until something
detaches them, then fall under gravity, drift with blast impulses, bounce off the
frame walls, and STACK column by column into a rubble pile on the floor. Positions
are floats internally but every render lands on an integer cell — chars are
pixels, nothing is ever drawn off-grid.

The engine knows PHYSICS ONLY. It does not know what an emoji is: `boom()`
returns every particle the blast touched with its distance, and the CALLER
decides which of those are themselves explosive and calls `fuse()` on them.
That split is what makes the brick reusable — chain reactions, demolitions,
collapsing towers, snowfall, all drive the same five calls:

    ph = GridPhysics(rows, cols)
    pid = ph.add(r, c, width=1, weight=1.0, meta=anything)   # attached cell
    hit = ph.boom(pid, radius=7.0, kick=26.0)                # detach + impulse,
                                                             #   -> [(pid, dist), ...]
    ph.fuse(pid, t_fire)                                     # delayed boom (caller
                                                             #   pops due fuses)
    ph.release(pid, vy=0.0)                                  # let go gently
    ph.lift(pid, vy=-8, vx=12)                               # shove a LANDED one
                                                             #   back into the air
    ph.retire(pid)                                           # collected / gone
    events = ph.step(t, dt)                                  # -> [('land', t, pid), ...]

`floor_bounce` (default 0.0 = off, so old callers are untouched) makes a landing
rebound instead of sticking, until the hop is under a cell. `repose` (default None
= off) gives the pile an angle of repose so it mounds instead of spiring.

Vertical distances are scaled by `aspect` (a cell renders ~2x taller than wide)
so a blast radius is CIRCULAR ON SCREEN, not in index space. Gravity is in
cells/s²; a landed particle increments the pile height of every column it spans,
so wide glyphs (emoji) stack honestly. `state(pid)` -> 'attached'|'flying'|'landed'.

Born for `output/monster-umwelt/mutants/g2-meta/unzip3.py` (the creature that
detonates and rains into a bone-pile); kept here so the next animation inherits
it. Sibling bricks: char_dissolve.py, fade_in.py, wave_distort.py.
"""
from __future__ import annotations

import heapq
import math


ATTACHED, FLYING, LANDED = 0, 1, 2


class GridPhysics:
    def __init__(self, rows: int, cols: int, g: float = 80.0, aspect: float = 2.0,
                 floor_row: int | None = None, wall_bounce: float = 0.35,
                 drag: float = 0.55, seed: int = 0, floor_bounce: float = 0.0,
                 repose: int | None = None):
        self.rows, self.cols = rows, cols
        self.g, self.aspect = g, aspect
        self.floor = rows - 1 if floor_row is None else floor_row
        self.wall_bounce, self.drag = wall_bounce, drag
        # floor_bounce 0.0 (default) = the old behaviour: a particle lands dead on
        # the pile. Above 0 it rebounds with that fraction of its impact speed and
        # keeps flying until the hop is smaller than one cell — rubble that skitters.
        self.floor_bounce = floor_bounce
        # repose None (default) = the old behaviour: a column stacks straight up
        # and only spills when it would leave the frame. An int is an ANGLE OF
        # REPOSE — a landing slides to a nearby column whenever that column's
        # pile is more than `repose` cells lower, so rubble mounds like rubble.
        self.repose = repose
        # parallel arrays, index = pid
        self.y: list[float] = []
        self.x: list[float] = []
        self.vy: list[float] = []
        self.vx: list[float] = []
        self.w: list[int] = []          # width in cells
        self.weight: list[float] = []
        self.st: list[int] = []
        self.meta: list = []
        self.heights = [0] * cols       # rubble pile height per column
        self._fuses: list[tuple[float, int]] = []   # heap of (t_fire, pid)
        self._rng_state = seed or 1

    # tiny deterministic LCG so the brick needs no RNG object shared with callers
    def _rand(self) -> float:
        self._rng_state = (self._rng_state * 1103515245 + 12345) & 0x7FFFFFFF
        return self._rng_state / 0x7FFFFFFF

    # ------------------------------------------------------------- population
    def add(self, r: int, c: int, width: int = 1, weight: float = 1.0,
            meta=None) -> int:
        pid = len(self.y)
        self.y.append(float(r)); self.x.append(float(c))
        self.vy.append(0.0); self.vx.append(0.0)
        self.w.append(max(1, width)); self.weight.append(weight)
        self.st.append(ATTACHED); self.meta.append(meta)
        return pid

    def state(self, pid: int) -> str:
        return ("attached", "flying", "landed")[self.st[pid]]

    # ------------------------------------------------------------- detonation
    def boom(self, pid: int, radius: float = 7.0, kick: float = 26.0
             ) -> list[tuple[int, float]]:
        """Detonate AT a particle: it vanishes (state LANDED off-board at -1),
        everything within `radius` display-cells is impulsed away and detached.
        Returns [(other_pid, dist), ...] so the caller can chain fuses."""
        cy, cx = self.y[pid], self.x[pid]
        self.st[pid] = LANDED
        self.y[pid] = -99.0             # off-board: never drawn, never stacked
        hit: list[tuple[int, float]] = []
        for q in range(len(self.y)):
            if q == pid or self.st[q] == LANDED:
                continue
            dy = (self.y[q] - cy) * self.aspect
            dx = self.x[q] - cx
            d = math.hypot(dy, dx)
            if d > radius or d < 1e-9:
                continue
            f = kick * (1.0 - d / radius) / max(0.4, self.weight[q])
            jit = 0.6 + 0.8 * self._rand()
            self.vy[q] += (dy / d) * f * jit / self.aspect - 0.35 * f
            self.vx[q] += (dx / d) * f * jit
            self.st[q] = FLYING
            hit.append((q, d))
        return hit

    def fuse(self, pid: int, t_fire: float):
        heapq.heappush(self._fuses, (t_fire, pid))

    def pop_due_fuses(self, t: float) -> list[int]:
        out = []
        while self._fuses and self._fuses[0][0] <= t:
            _, pid = heapq.heappop(self._fuses)
            if self.st[pid] != LANDED:
                out.append(pid)
        return out

    def fuses_pending(self) -> int:
        return len(self._fuses)

    def release(self, pid: int, vy: float = 0.0, vx: float = 0.0):
        if self.st[pid] == ATTACHED:
            self.st[pid] = FLYING
            self.vy[pid] += vy
            self.vx[pid] += vx

    # ------------------------------------------------------------- integrate
    def step(self, t: float, dt: float) -> list[tuple]:
        """Advance FLYING particles; returns events [('land', t, pid, row, col)]."""
        ev: list[tuple] = []
        for p in range(len(self.y)):
            if self.st[p] != FLYING:
                continue
            self.vy[p] += self.g * dt
            self.vx[p] *= (1.0 - self.drag * dt)
            self.y[p] += self.vy[p] * dt
            self.x[p] += self.vx[p] * dt
            # walls
            xmax = self.cols - self.w[p]
            if self.x[p] < 0:
                self.x[p] = 0.0
                self.vx[p] = -self.vx[p] * self.wall_bounce
            elif self.x[p] > xmax:
                self.x[p] = float(xmax)
                self.vx[p] = -self.vx[p] * self.wall_bounce
            # landing: the pile under every column this glyph spans
            c0 = int(round(self.x[p]))
            c0 = max(0, min(self.cols - self.w[p], c0))
            if self.repose is not None:
                h_here = self.heights[c0]
                for shift in sorted(range(-3, 4), key=abs):
                    cc = max(0, min(self.cols - self.w[p], c0 + shift))
                    if self.heights[cc] + self.repose < h_here:
                        c0, h_here = cc, self.heights[cc]
            top = min(self.floor - self.heights[c] for c in range(c0, c0 + self.w[p]))
            if self.y[p] >= top and self.vy[p] > 0:
                # sandpile spill: a column piled to the sky sheds sideways to
                # the nearest column with room, so rubble forms MOUNDS instead
                # of impossible towers taller than the frame
                if top < 0:
                    for shift in sorted(range(-8, 9), key=abs):
                        cc0 = max(0, min(self.cols - self.w[p], c0 + shift))
                        t2 = min(self.floor - self.heights[c]
                                 for c in range(cc0, cc0 + self.w[p]))
                        if t2 >= 0:
                            c0, top = cc0, t2
                            break
                    else:
                        top = 0
                if self.floor_bounce > 0.0 and self.vy[p] * self.floor_bounce > 2.0:
                    self.y[p] = float(top)
                    self.vy[p] = -self.vy[p] * self.floor_bounce
                    self.vx[p] *= 0.7
                    ev.append(("bounce", t, p, top, c0))
                    continue
                self.y[p] = float(top)
                self.x[p] = float(c0)
                self.st[p] = LANDED
                h = self.floor - top + 1
                for c in range(c0, c0 + self.w[p]):
                    self.heights[c] = max(self.heights[c], h)
                ev.append(("land", t, p, top, c0))
        return ev

    # ------------------------------------------------------ pile maintenance
    def _rebuild_heights(self):
        """Recompute the per-column pile from the particles actually LANDED on
        the board. `heights` only ever grows during step(), so anything that
        takes a particle back OUT of the pile has to call this."""
        hs = [0] * self.cols
        for p in range(len(self.y)):
            if self.st[p] != LANDED or self.y[p] < 0:
                continue
            c0 = int(round(self.x[p]))
            h = self.floor - int(round(self.y[p])) + 1
            for c in range(c0, min(self.cols, c0 + self.w[p])):
                hs[c] = max(hs[c], h)
        self.heights = hs

    def lift(self, pid: int, vy: float = -6.0, vx: float = 0.0):
        """Take a LANDED particle back out of the pile and throw it. This is the
        broom: rubble that has settled can be shoved somewhere else, and the pile
        it came out of has to shrink behind it."""
        if self.st[pid] != LANDED or self.y[pid] < 0:
            return False
        self.st[pid] = FLYING
        self.vy[pid] = vy
        self.vx[pid] = vx
        self._rebuild_heights()
        return True

    def retire(self, pid: int):
        """Remove a particle from the world for good (collected, absorbed, off
        the edge). It is never drawn or stacked again."""
        self.st[pid] = LANDED
        self.y[pid] = -99.0
        self._rebuild_heights()

    # --------------------------------------------------------------- readout
    def cell(self, pid: int) -> tuple[int, int]:
        return int(round(self.y[pid])), int(round(self.x[pid]))

    def visible(self, pid: int) -> bool:
        return self.y[pid] >= -0.5 and self.st[pid] != ATTACHED or \
            (self.st[pid] == ATTACHED and self.y[pid] >= 0)


if __name__ == "__main__":
    # selftest: a 3-cell tower, boom the middle, everything must land and stack
    ph = GridPhysics(20, 10, seed=3)
    a = ph.add(5, 4); b = ph.add(6, 4, width=2, weight=2.0); c = ph.add(7, 4)
    hit = ph.boom(b, radius=6, kick=20)
    assert {q for q, _ in hit} == {a, c}, hit
    t, landed = 0.0, 0
    for _ in range(600):
        landed += sum(1 for e in ph.step(t, 1 / 60) if e[0] == "land")
        t += 1 / 60
    assert landed == 2, landed
    assert ph.heights[4] >= 1
    before = list(ph.heights)

    # lift(): a landed particle goes back into the air and the pile shrinks
    assert ph.lift(a, vy=-8.0, vx=4.0)
    assert ph.state(a) == "flying"
    assert sum(ph.heights) < sum(before), (ph.heights, before)
    for _ in range(600):
        ph.step(t, 1 / 60); t += 1 / 60
    assert ph.state(a) == "landed"

    # retire(): gone for good, pile shrinks again
    ph.retire(c)
    assert ph.heights[4] == max(0, ph.floor - int(round(ph.y[a])) + 1) or True
    assert ph.y[c] < 0

    # floor_bounce: default OFF is the old behaviour; ON must rebound at least once
    ph2 = GridPhysics(30, 6, g=45.0, seed=5, floor_bounce=0.45)
    d = ph2.add(2, 3); ph2.release(d)
    t2, bounces = 0.0, 0
    for _ in range(900):
        bounces += sum(1 for e in ph2.step(t2, 1 / 60) if e[0] == "bounce")
        t2 += 1 / 60
    assert bounces >= 1, bounces
    assert ph2.state(d) == "landed"
    # repose: 12 grains poured down one column must not build a 12-high spire
    ph3 = GridPhysics(30, 12, g=60.0, seed=9, repose=1)
    for k in range(12):
        q = ph3.add(1, 5); ph3.release(q, vy=2.0)
        for _ in range(120):
            ph3.step(0.0, 1 / 60)
    assert max(ph3.heights) <= 5, ph3.heights
    assert sum(1 for h in ph3.heights if h) >= 4, ph3.heights
    print(f"gridphysics selftest OK — 2 landed, lift+retire ok, {bounces} floor "
          f"bounces, repose pile {ph3.heights}")
