// Desk mosh: shards of the live desktop, moved by a clock. See desk_mosh.h and docs/desk-mosh.md.
#include "desk_mosh.h"

#define Uses_TWindow
#define Uses_TFrame
#define Uses_TGroup
#define Uses_TDeskTop
#define Uses_TProgram
#define Uses_TScreen
#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TDialog
#define Uses_THardwareInfo
#include <tvision/tv.h>

#include "ega_theme.h"
#include "resident_senses.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <sstream>
#include <vector>

extern TView* TheTopView;

namespace deskmosh {
namespace {

using Clock = std::chrono::steady_clock;
using Cells = std::vector<TScreenCell>;

// ---------------------------------------------------------------- settings and state

enum Way { Wrap, Tear, Mosh, Bleed, Raw, All };
const char* kWayNames[] = {"wrap", "tear", "mosh", "bleed", "raw", "all"};

struct Hit {
    bool kick = false, snare = false, tick = false, rat = false, metal = false, accent = false;
    int step = -1;   // position in the bar (0-15), -1 = count on from the last
};

struct Settings {
    Way way = All;
    int amount = 70;     // 0..100
    int tempo = 128;     // BPM of the internal clock; 0 = steps only from the TR-808 or mosh_step
    uint32_t seed = 1;
    int count = 240;     // most shards at once
    std::string scope;   // "" = desktop, else a window id
} g_set;

bool g_on = false;
std::string g_by;                 // who turned it on
uint64_t g_step = 0;              // steps run since on
uint64_t g_opened = 0, g_closed = 0;
std::deque<Hit> g_queue;          // steps waiting for tick()
Clock::time_point g_nextAt;       // internal clock
Clock::time_point g_last808{};    // last TR-808 step seen
int g_busy = 0;                   // inside a shard's event handler: no windows open or close
Clock::time_point g_lastEsc{};
double g_stepMs = 0, g_stepMsAvg = 0, g_loopMsAvg = 0;
Clock::time_point g_lastTick{};

// Film counters (catmosh player_main.js), advanced by the hits.
struct Beat {
    int bar = 0, pos = -1, nk = 0, tearSeed = 1, stutH = 1, stutSeed = 1, blockSeed = 1, vecSeed = 1, rot = 0;
    int zoom = 1, zx = 0, zy = 0, stride = 0, phase = 0, roll = 0, scroll = 0, rawStride = 80, rawLeft = 0, rj = 0;
    int sinceKick = 99, sinceSnare = 99, sinceMetal = 99, sinceRat = 99;
    bool accent = false;
    int active = 0;
} B;

const int WRAPS[] = {1, -1, 2, 7, -3, 13, -13, 27, 80, -41};
const int NWRAPS = 10;

uint32_t mix(uint32_t h) { h ^= h >> 13; h *= 1274126177u; h ^= h >> 16; return h; }
// Hash of three ints to [0,1): the film's h3, salted with the seed.
double h3(int a, int b, int c) {
    uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u + (uint32_t)c * 2246822519u + g_set.seed * 3266489917u;
    return mix(h) / 4294967296.0;
}
int imod(int i, int n) { return n <= 0 ? 0 : ((i % n) + n) % n; }
double amt() { return g_set.amount / 100.0; }

// ---------------------------------------------------------------- colours

int lumOf(int idx) {
    uint32_t v = (uint32_t) ww::egaRGB(idx);
    return (int)(((v >> 16) & 255) * 0.3 + ((v >> 8) & 255) * 0.59 + (v & 255) * 0.11);
}
int nearestEga(TColorRGB c) {
    uint32_t v = (uint32_t) c;
    int r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255, best = 0, bd = 1 << 30;
    for (int i = 0; i < 16; ++i) {
        uint32_t e = (uint32_t) ww::egaRGB(i);
        int dr = r - (int)((e >> 16) & 255), dg = g - (int)((e >> 8) & 255), db = b - (int)(e & 255);
        int d = dr * dr + dg * dg + db * db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
int idxOf(TColor c, bool fg) {
    if (c.isBIOS()) return c.asBIOS() & 15;
    if (c.isRGB()) return nearestEga(c.asRGB());
    if (c.isXTerm()) {
        uint8_t x = c.asXTerm();
        if (x >= 16) x = TColorConversion::XTerm256toXTerm16(x);
        return TColorConversion::XTerm16toBIOS(x) & 15;
    }
    return fg ? 7 : 0;
}
int fgIdx(const TScreenCell& c) { return idxOf(c.attribute.getForeground(), true); }
int bgIdx(const TScreenCell& c) { return idxOf(c.attribute.getBackground(), false); }
void setColours(TScreenCell& c, int fg, int bg) {
    c.attribute.setForeground(TColor(ww::egaRGB(imod(fg, 16))));
    c.attribute.setBackground(TColor(ww::egaRGB(imod(bg, 16))));
}
const char* SHADE[] = {" ", "░", "▒", "▓", "█"};
void setGlyph(TScreenCell& c, const char* utf8) { c.character.initWithMultiByteChar(TStringView(utf8)); }
TScreenCell blankCell() {
    TScreenCell c{};
    setGlyph(c, " ");
    setColours(c, 7, 0);
    return c;
}
// A wide character cut from its trail (or a trail cut from its lead) draws as a space.
void fixWide(TScreenCell* row, int w) {
    for (int x = 0; x < w; ++x) {
        TScreenCell& c = row[x];
        if (c.character.isWideCharTrail() && (x == 0 || !row[x - 1].character.isWide())) setGlyph(c, " ");
        else if (c.character.isWide() && (x + 1 >= w || !row[x + 1].character.isWideCharTrail())) setGlyph(c, " ");
    }
}

// ---------------------------------------------------------------- the material: the true desktop

struct Grid {
    int w = 0, h = 0;
    Cells c;
    const TScreenCell& at(int x, int y) const { return c[(size_t)imod(y, h) * w + imod(x, w)]; }
    const TScreenCell& flat(long i) const { long n = (long)c.size(); return c[(size_t)(((i % n) + n) % n)]; }
};

Grid g_bg;   // the desktop as it was when the mosh began (windows included)

std::vector<TWindow*> deskWindows() {
    std::vector<TWindow*> out;
    TDeskTop* d = TProgram::deskTop;
    if (!d || !d->first()) return out;
    TView* f = d->first();
    TView* v = f;
    do {
        if (auto* w = dynamic_cast<TWindow*>(v)) out.push_back(w);
        v = v->next;
    } while (v != f);
    return out;
}
std::vector<TWindow*> realWindows() {
    std::vector<TWindow*> out;
    for (TWindow* w : deskWindows())
        if (!isShard(w) && (w->state & sfVisible)) out.push_back(w);
    return out;
}

Grid captureScreenDesk() {
    Grid g;
    TDeskTop* d = TProgram::deskTop;
    if (!d || !TScreen::screenBuffer) return g;
    TPoint o = d->makeGlobal(TPoint{0, 0});
    g.w = d->size.x; g.h = d->size.y;
    g.c.assign((size_t)g.w * g.h, blankCell());
    int sw = TScreen::screenWidth, sh = TScreen::screenHeight;
    for (int y = 0; y < g.h; ++y)
        for (int x = 0; x < g.w; ++x) {
            int sx = o.x + x, sy = o.y + y;
            if (sx >= 0 && sy >= 0 && sx < sw && sy < sh) g.c[(size_t)y * g.w + x] = TScreen::screenBuffer[sy * sw + sx];
        }
    return g;
}

// The desktop as the real windows draw it now: the first capture, with each real
// window's own buffer laid on it back to front. Shards are never in it.
Grid trueDesk() {
    Grid g = g_bg;
    if (g.c.empty()) return g;
    auto wins = realWindows();
    for (auto it = wins.rbegin(); it != wins.rend(); ++it) {
        TWindow* w = *it;
        if (!w->buffer) continue;
        TRect r = w->getBounds();
        for (int y = 0; y < w->size.y; ++y)
            for (int x = 0; x < w->size.x; ++x) {
                int dx = r.a.x + x, dy = r.a.y + y;
                if (dx < 0 || dy < 0 || dx >= g.w || dy >= g.h) continue;
                g.c[(size_t)dy * g.w + dx] = w->buffer[y * w->size.x + x];
            }
    }
    return g;
}

// ---------------------------------------------------------------- shards

class TMoshView : public TView {
public:
    explicit TMoshView(const TRect& r) : TView(r) { growMode = gfGrowHiX | gfGrowHiY; }
    Cells cells;
    void draw() override {
        int w = size.x, h = size.y;
        if ((int)cells.size() < w * h) { TView::draw(); return; }
        for (int y = 0; y < h; ++y) writeBuf(0, y, w, 1, &cells[(size_t)y * w]);
    }
};

class TMoshShard;
std::map<int, TMoshShard*> g_pool;       // live shards the mosh drives, by slot
std::vector<TMoshShard*> g_held;         // shards a person grabbed: left where they were put

TFrame* noFrame(TRect) { return nullptr; }

class TMoshShard : public TWindow {
public:
    TMoshShard(const TRect& r, bool framed, const std::string& title, int slot)
        : TWindowInit(framed ? &TWindow::initFrame : &noFrame), TWindow(r, title, wnNoNumber), slot_(slot), framed_(framed) {
        state &= ~sfShadow;
        options &= ~(ofSelectable | ofTopSelect | ofTileable);
        flags = framed ? (wfMove | wfClose) : 0;
        growMode = 0;
        TRect in = getExtent();
        if (framed) in.grow(-1, -1);
        view_ = new TMoshView(in);
        insert(view_);
        ++g_opened;
    }
    ~TMoshShard() override {
        auto it = g_pool.find(slot_);
        if (it != g_pool.end() && it->second == this) g_pool.erase(it);
        g_held.erase(std::remove(g_held.begin(), g_held.end(), this), g_held.end());
        ++g_closed;
    }
    bool framed() const { return framed_; }
    // Shards may be one cell; TWindow's own limits (16x6) would stretch them on every move.
    void sizeLimits(TPoint& min, TPoint& max) override { min.x = min.y = 1; max.x = max.y = 32000; }
    void setCells(Cells&& c) { view_->cells = std::move(c); view_->drawView(); }
    void handleEvent(TEvent& ev) override;
private:
    int slot_;
    bool framed_;
    TMoshView* view_;
};

// The view under a desktop point, below the shard "from", that is not a shard.
TView* viewBelow(TView* from, TPoint where) {
    TGroup* o = from->owner;
    if (!o) return nullptr;
    TPoint p = o->makeLocal(where);
    TView* f = o->first();
    for (TView* v = from->next; v && v != f; v = v->next)
        if ((v->state & sfVisible) && !isShard(v) && v->getBounds().contains(p)) return v;
    return nullptr;
}

void TMoshShard::handleEvent(TEvent& ev) {
    if (ev.what == evMouseDown) {
        ++g_busy;
        if (ev.mouse.buttons & mbRightButton) {
            // A person grabs the shard: it leaves the mosh and stays where it is put.
            auto it = g_pool.find(slot_);
            if (it != g_pool.end() && it->second == this) { g_pool.erase(it); g_held.push_back(this); }
            TRect lim = owner->getExtent();
            lim.grow(size.x, size.y);
            dragView(ev, dmDragMove, lim, size, size);
            clearEvent(ev);
        } else if (framed_ && ev.mouse.where.y == makeGlobal(TPoint{0, 0}).y) {
            TWindow::handleEvent(ev);   // the frame: drag or close as any window
        } else if (TView* v = viewBelow(this, ev.mouse.where)) {
            v->handleEvent(ev);         // a left click goes through to the real desktop
        }
        --g_busy;
        return;
    }
    TWindow::handleEvent(ev);
}

// ---------------------------------------------------------------- what the mosh changes on real windows

struct RealChange {
    bool moved = false; TRect orig, set;
    bool coloured = false; ww::WinColour origC, setC;
};
std::map<TWindow*, RealChange> g_real;

bool alive(TWindow* w) {
    for (TWindow* x : deskWindows()) if (x == w) return true;
    return false;
}

void restoreReal() {
    for (auto& kv : g_real) {
        TWindow* w = kv.first;
        if (!alive(w)) continue;
        RealChange& c = kv.second;
        if (c.moved && w->getBounds() == c.set) w->locate(c.orig);
        if (c.coloured) {
            ww::WinColour now = ww::windowColour(w);
            if (now.bg == c.setC.bg && now.ink == c.setC.ink) { ww::setWindowColour(w, c.origC); ww::redrawDeep(w); }
        }
    }
    g_real.clear();
}

void resizeReal(TWindow* w, const TRect& r) {
    RealChange& c = g_real[w];
    if (!c.moved) { c.moved = true; c.orig = w->getBounds(); }
    w->locate((TRect&) r);
    c.set = w->getBounds();
}

void colourReal(TWindow* w, int bg) {
    RealChange& c = g_real[w];
    if (!c.coloured) { c.coloured = true; c.origC = ww::windowColour(w); }
    c.setC = ww::WinColour{bg, -1};
    ww::setWindowColour(w, c.setC);
    ww::redrawDeep(w);
}

// ---------------------------------------------------------------- specs: what each way wants on screen this step

struct Spec {
    int slot = 0;
    TRect r;          // region cells
    bool framed = false;
    bool keep = false;  // leave last step's shard as it is
    std::string title;
    Cells cells;      // the inside, (w-2)x(h-2) when framed
};

TRect g_region;       // where the mosh works, desktop cells
TPoint g_caret{-1, -1};

TPoint caretPoint() {
    TDeskTop* d = TProgram::deskTop;
    if (!d || !d->current) return {-1, -1};
    TView* v = d->current;
    while (auto* g = dynamic_cast<TGroup*>(v)) { if (!g->current) break; v = g->current; }
    if (!(v->state & sfCursorVis)) return {-1, -1};
    return d->makeLocal(v->makeGlobal(v->cursor));
}

// A frameless shard at r showing cells picked by f(x, y) (x, y inside the shard).
template <class F>
Spec cut(int slot, TRect r, F f) {
    Spec s;
    s.slot = slot; s.r = r;
    int w = r.b.x - r.a.x, h = r.b.y - r.a.y;
    s.cells.resize((size_t)std::max(0, w * h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) s.cells[(size_t)y * w + x] = f(x, y);
        fixWide(&s.cells[(size_t)y * w], w);
    }
    return s;
}

double env(int since, double k) { return std::exp(-since * k); }

// Width of the segments a band of rows is cut into, so the number of shards follows the budget.
int segWidth(int bands, int width) {
    int budget = std::max(1, (int)(g_set.count * (0.35 + 0.65 * amt())));
    int perBand = std::max(1, budget / std::max(1, bands));
    return std::max(2, (width + perBand - 1) / perBand);
}

// wrap: rows read at the wrong width, so the desktop slips and tiles; each kick a new width.
void wayWrap(const Grid& t, std::vector<Spec>& out) {
    int W = t.w, H = t.h;
    long n = (long)W * H;
    int stride = W + B.stride;
    int bandH = 3;
    int bands = (H + bandH - 1) / bandH;
    int sw = segWidth(bands, W);
    int slot = 0;
    for (int by = 0; by < bands; ++by) {
        int y0 = by * bandH, h = std::min(bandH, H - y0);
        for (int x0 = 0; x0 < W; x0 += sw) {
            int w = std::min(sw, W - x0);
            out.push_back(cut(slot++, TRect(x0, y0, x0 + w, y0 + h), [&](int x, int y) {
                return t.flat((long)(y0 + y) * stride + x0 + x + B.phase);
            }));
        }
    }
    (void) n;
}

// tear: bands shear sideways on kicks, rows repeat on snares, ratchets roll the frame.
void wayTear(const Grid& t, std::vector<Spec>& out) {
    int W = t.w, H = t.h;
    double kick = env(B.sinceKick, 0.9), sn = env(B.sinceSnare, 0.8);
    double tearAmp = (4 + 40 * kick + (B.accent ? 10 : 0)) * amt() * (W / 80.0);
    int bandH = 2 + (B.tearSeed % 4);
    int stut = sn > 0.3 ? B.stutH : 1;
    int bands = (H + bandH - 1) / bandH;
    int sw = segWidth(bands, W);
    int slot = 0;
    for (int by = 0; by < bands; ++by) {
        int y0 = by * bandH, h = std::min(bandH, H - y0);
        int off = (int)std::lround((h3(by, B.tearSeed, 7) - 0.5) * 2 * tearAmp);
        for (int x0 = 0; x0 < W; x0 += sw) {
            int w = std::min(sw, W - x0);
            int jitter = (int)std::lround((h3(x0, by, B.tearSeed + 31) - 0.5) * 2 * kick * 3);
            int nx = x0 + off + jitter;
            out.push_back(cut(slot++, TRect(nx, y0, nx + w, y0 + h), [&](int x, int y) {
                int yy = y0 + y;
                if (stut > 1 && h3(yy / (stut * 3), B.stutSeed, 9) < 0.6) yy = yy - (yy % stut) - (yy / stut) % 2;
                return t.at(x0 + x, yy + B.roll);
            }));
        }
    }
}

struct BlockState { int dx = 0, dy = 0; };
std::map<int, BlockState> g_blocks;
Grid g_shown;   // what the mosh way showed last step (region cells)

// mosh: the film's datamosh. Most blocks are not redrawn: they carry last step's shown
// cells along their own direction, so shapes smear into trails. Refreshed blocks take
// the true desktop, half of them keeping the old glyphs in the new colours. A few blocks
// also drift as windows; in a smear (metal) blocks read at half pitch.
void wayMosh(const Grid& t, std::vector<Spec>& out) {
    int W = t.w, H = t.h;
    if (g_shown.w != W || g_shown.h != H) g_shown = t;
    int budget = std::max(8, (int)(g_set.count * (0.35 + 0.65 * amt())));
    int area = W * H;
    int bh = std::max(2, (int)std::lround(std::sqrt(area / (double)budget / 2.0)));
    int bw = bh * 2;
    double kick = env(B.sinceKick, 0.9), met = env(B.sinceMetal, 0.5);
    double refresh = (0.04 + 0.5 * kick) * (1.2 - amt() * 0.6);
    Grid next = g_shown;
    int slot = 0;
    for (int by = 0; by * bh < H; ++by)
        for (int bx = 0; bx * bw < W; ++bx, ++slot) {
            int hx = bx * bw, hy = by * bh, w = std::min(bw, W - hx), h = std::min(bh, H - hy);
            bool again = h3(bx, by, (int)(g_step >> 1)) < refresh;
            int vx = (int)(h3(bx, by, B.vecSeed) * 5) - 2, vy = (int)(h3(by, bx, B.vecSeed + 3) * 3) - 1;
            bool eh = met > 0.1 && h3(bx, by, B.blockSeed + 9) < met * 0.5;
            bool ev = met > 0.1 && !eh && h3(by, bx, B.blockSeed + 8) < met * 0.4;
            bool oldGlyphs = h3(bx, by, B.nk + 77) < 0.5;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    int X = hx + x, Y = hy + y;
                    TScreenCell c;
                    if (again) {
                        c = t.at(X, Y);
                        if (oldGlyphs) c.character = g_shown.at(X, Y).character;
                    } else {
                        int sx = eh ? hx + (x >> 1) + 2 : X, sy = ev ? hy + (y >> 1) + 1 : Y;
                        c = g_shown.at(sx - vx, sy - vy);
                    }
                    next.c[(size_t)Y * W + X] = c;
                }
            BlockState& s = g_blocks[slot];
            if (again) s.dx = s.dy = 0;
            else if (h3(bx, by, 4242) < 0.12 * amt()) { s.dx += vx; s.dy += vy; }   // this block drifts as a window
            int px = hx + s.dx, py = hy + s.dy;
            out.push_back(cut(slot, TRect(px, py, px + w, py + h), [&](int x, int y) {
                return next.at(hx + x, hy + y);
            }));
        }
    g_shown = std::move(next);
}

// bleed: colour changes dropped so colours run on, palette rotates in bands on snares,
// glyphs decay to shade blocks.
void wayBleed(const Grid& t, std::vector<Spec>& out) {
    int W = t.w, H = t.h;
    double sn = env(B.sinceSnare, 0.8), met = env(B.sinceMetal, 0.5);
    double drop = (0.25 + 0.6 * sn) * amt();
    double decay = (0.08 + 0.5 * met + 0.25 * sn) * amt();
    int bandH = 4, bands = (H + bandH - 1) / bandH;
    int sw = segWidth(bands, W);
    int slot = 0;
    for (int by = 0; by < bands; ++by) {
        int y0 = by * bandH, h = std::min(bandH, H - y0);
        int rot = h3(by, B.rot, 11) < 0.5 ? B.rot : 0;
        for (int x0 = 0; x0 < W; x0 += sw) {
            int w = std::min(sw, W - x0);
            std::vector<TColorAttr> last((size_t)h);
            std::vector<bool> has((size_t)h, false);
            out.push_back(cut(slot++, TRect(x0, y0, x0 + w, y0 + h), [&](int x, int y) {
                int X = x0 + x, Y = y0 + y;
                TScreenCell c = t.at(X, Y);
                int f = fgIdx(c), b = bgIdx(c);
                if (rot) { f = (f + rot) % 16; b = (b + rot) % 16; setColours(c, f, b); }
                if (h3(X >> 2, Y >> 1, B.blockSeed + B.stutSeed) < decay) {
                    setGlyph(c, SHADE[std::min(4, lumOf(b) * 5 / 256)]);
                    setColours(c, b, b + 1);
                }
                // a colour change that never arrives: the last colour runs on
                if (has[y] && h3(X, Y, (int)(g_step >> 1)) < drop) c.attribute = last[y];
                last[y] = c.attribute; has[y] = true;
                return c;
            }));
        }
    }
}

// raw: zoomed cells, rows left stale, scrolling on snares, bursts of the desktop written
// as one stream at the wrong width on ratchets, and a framed zoomed copy of a window.
void wayRaw(const Grid& t, std::vector<Spec>& out) {
    int W = t.w, H = t.h;
    long n = (long)W * H;
    int bandH = 2, bands = (H + bandH - 1) / bandH;
    int sw = segWidth(bands, W);
    int z = B.zoom;
    bool burst = B.rawLeft > 0;
    long start = (long)(h3((int)g_step, 1, 1) * n);
    int slot = 0;
    for (int by = 0; by < bands; ++by) {
        int y0 = by * bandH, h = std::min(bandH, H - y0);
        bool stale = h3(by, (int)(g_step >> 1), 13) < 0.3 * (0.4 + 0.6 * amt());
        for (int x0 = 0; x0 < W; x0 += sw, ++slot) {
            int w = std::min(sw, W - x0);
            if (stale && g_pool.count(slot)) {   // rows left as they were
                Spec s; s.slot = slot; s.r = TRect(x0, y0, x0 + w, y0 + h); s.keep = true;
                out.push_back(std::move(s));
                continue;
            }
            out.push_back(cut(slot, TRect(x0, y0, x0 + w, y0 + h), [&](int x, int y) {
                int X = x0 + x, Y = y0 + y + B.scroll;
                if (burst) return t.flat(start + (long)Y * B.rawStride + X);
                if (z > 1) return t.at(X / z + B.zx, Y / z + B.zy);
                return t.at(X + B.phase, Y);
            }));
        }
    }
    auto wins = realWindows();
    if (!wins.empty() && z > 1) {
        TWindow* src = wins[(size_t)B.nk % wins.size()];
        TRect sr = src->getBounds();
        sr.move(-g_region.a.x, -g_region.a.y);
        int w = std::min(W - 2, (sr.b.x - sr.a.x) * z / 2 + 2), h = std::min(H - 2, (sr.b.y - sr.a.y) * z / 2 + 2);
        if (w > 6 && h > 3) {
            int x0 = (int)(h3(B.nk, 5, 6) * std::max(1, W - w)), y0 = (int)(h3(B.nk, 7, 8) * std::max(1, H - h));
            Spec s = cut(100000, TRect(x0 + 1, y0 + 1, x0 + w - 1, y0 + h - 1), [&](int x, int y) {
                return t.at(sr.a.x + x / z, sr.a.y + y / z);
            });
            s.r = TRect(x0, y0, x0 + w, y0 + h);
            s.framed = true;
            s.title = "x" + std::to_string(z);
            out.push_back(std::move(s));
        }
    }
}

// ---------------------------------------------------------------- the screen layer (optional)
//
// After the windows, a filter over the desktop rows of the screen itself, for what windows
// cannot do: colour runs and decay across window edges, rows read at the wrong width
// across the whole desktop. TWwdosApp::getEvent calls screenBefore() before the base
// getEvent, which flushes the screen to the terminal, and screenAfter() when it returns;
// tick() also calls screenAfter() first, so idle work, IPC and look_study see the true
// screen. Only cells that still hold the value the filter wrote are put back, so a view
// that drew in between keeps its drawing.

bool g_screenOn = false;
int g_screenDepth = 0;
Grid g_prevShown;          // what the filter showed at the last step
Grid g_stepShown;          // what it shows during this step
uint64_t g_screenStep = ~0ull;
std::vector<std::pair<size_t, TScreenCell>> g_saved;   // index, true cell
std::vector<TScreenCell> g_wrote;                       // the cell written there

int screenTop() { return TProgram::deskTop ? TProgram::deskTop->origin.y : 1; }
int screenRows() { return TProgram::deskTop ? TProgram::deskTop->size.y : 0; }

Grid screenGrid() {
    Grid g;
    int W = TScreen::screenWidth, top = screenTop(), H = screenRows();
    if (!TScreen::screenBuffer || W <= 0 || H <= 0) return g;
    g.w = W; g.h = H;
    g.c.assign(TScreen::screenBuffer + (size_t)top * W, TScreen::screenBuffer + (size_t)(top + H) * W);
    return g;
}

// The film's compose(), for one step's parameters, over the true screen S.
Grid screenCompose(const Grid& S) {
    Grid N = S;
    int W = S.w, H = S.h, p = B.active;
    if (g_prevShown.w != W || g_prevShown.h != H) g_prevShown = S;
    const Grid& P = g_prevShown;
    double kick = env(B.sinceKick, 0.9), sn = env(B.sinceSnare, 0.8), met = env(B.sinceMetal, 0.5), a = amt();
    double tearAmp = p == Tear ? (4 + 30 * kick) * a : p == Raw ? 12 * kick * a : 0;
    int bandH = p == Tear ? 2 + (B.tearSeed % 5) : 3;
    int stride = W + (p == Wrap ? B.stride : 0);
    double decay = p == Bleed ? (0.08 + 0.5 * met + 0.25 * sn) * a * 0.6 : 0;
    double drop = p == Bleed ? (0.25 + 0.6 * sn) * a : p == Raw ? 0.3 * a : 0;
    int z = p == Raw ? B.zoom : 1;
    for (int y = 0; y < H; ++y) {
        if (p == Raw && h3(y >> 1, (int)(g_step >> 3), 13) < 0.3 * a) {   // rows left as they were
            for (int x = 0; x < W; ++x) N.c[(size_t)y * W + x] = P.at(x, y);
            continue;
        }
        int ys = y;
        if (p == Tear && sn > 0.3 && B.stutH > 1 && h3(y / (B.stutH * 3), B.stutSeed, 9) < 0.6) ys = y - (y % B.stutH);
        int off = (p == Wrap ? B.phase : 0) + (tearAmp ? (int)std::lround((h3(ys / bandH, B.tearSeed, 7) - 0.5) * 2 * tearAmp) : 0);
        int rot = (p == Bleed) && h3(y / 4, B.rot, 11) < 0.5 ? B.rot : 0;
        bool have = false;
        TColorAttr last{};
        for (int x = 0; x < W; ++x) {
            TScreenCell c;
            if (z > 1) c = S.at(x / z + B.zx, (ys + B.scroll) / z + B.zy);
            else if (p == Wrap) c = S.flat((long)ys * stride + x + off);
            else c = S.at(x + off, ys + (p == Raw ? B.scroll : 0));
            if (rot) setColours(c, fgIdx(c) + rot, bgIdx(c) + rot);
            if (decay && h3(x >> 2, y >> 1, B.blockSeed + B.stutSeed) < decay) {
                int b = bgIdx(c);
                setGlyph(c, SHADE[std::min(4, lumOf(b) * 5 / 256)]);
                setColours(c, b, b + 1);
            }
            if (have && drop && h3(x, y, (int)(g_step >> 1)) < drop) c.attribute = last;
            last = c.attribute; have = true;
            N.c[(size_t)y * W + x] = c;
        }
    }
    if (p == Mosh) {
        double refresh = 0.04 + 0.5 * kick;
        for (int by = 0; by * 4 < H; ++by)
            for (int bx = 0; bx * 8 < W; ++bx) {
                if (h3(bx, by, (int)(g_step >> 1) + 999) < refresh) continue;
                int vx = (int)(h3(bx, by, B.vecSeed) * 5) - 2, vy = (int)(h3(by, bx, B.vecSeed + 3) * 3) - 1;
                for (int y = by * 4; y < std::min(H, by * 4 + 4); ++y)
                    for (int x = bx * 8; x < std::min(W, bx * 8 + 8); ++x) N.c[(size_t)y * W + x] = P.at(x - vx, y - vy);
            }
    }
    for (int y = 0; y < H; ++y) fixWide(&N.c[(size_t)y * W], W);
    return N;
}

void screenAfterImpl() {
    if (g_saved.empty() || !TScreen::screenBuffer) { g_saved.clear(); g_wrote.clear(); return; }
    size_t n = (size_t)TScreen::screenWidth * TScreen::screenHeight;
    for (size_t k = 0; k < g_saved.size(); ++k) {
        size_t i = g_saved[k].first;
        if (i < n && TScreen::screenBuffer[i] == g_wrote[k]) TScreen::screenBuffer[i] = g_saved[k].second;
    }
    g_saved.clear();
    g_wrote.clear();
}

void screenRefreshAll() {
    if (!TScreen::screenBuffer) return;
    int W = TScreen::screenWidth;
    for (int y = 0; y < TScreen::screenHeight; ++y) THardwareInfo::screenWrite(0, y, TScreen::screenBuffer + (size_t)y * W, W);
}

// ---------------------------------------------------------------- the beat

void advance(const Hit& hit) {
    int pos = hit.step >= 0 ? hit.step : (B.pos + 1) % 16;
    if (pos <= B.pos) {
        B.bar++;
        if (B.active == Wrap) B.stride = 0;   // each bar starts whole, then slips
    }
    B.pos = pos;
    B.accent = hit.accent;
    B.sinceKick++; B.sinceSnare++; B.sinceMetal++; B.sinceRat++;
    if (B.rawLeft > 0) B.rawLeft--;
    int p = B.active;
    if (hit.kick) {
        B.sinceKick = 0; B.nk++; B.tearSeed = B.nk;
        if (p == Wrap) B.stride = WRAPS[(B.nk + g_set.seed) % NWRAPS];
        if (p == Raw) {
            const int zs[] = {1, 2, 1, 3, 2};
            B.zoom = zs[B.nk % 5];
            B.zx = (int)(h3(B.nk, 1, 2) * g_bg.w); B.zy = (int)(h3(B.nk, 3, 4) * g_bg.h);
        }
    }
    if (hit.snare) {
        B.sinceSnare = 0; B.stutH = 2 + (B.stutSeed % 4); B.stutSeed++;
        B.rot = (B.rot + 1 + (int)(h3(B.stutSeed, 5, 6) * 3)) % 16;
        if (B.rot == 0) B.rot = 1;
        if (p == Raw) B.scroll += 1 + (int)(h3(B.stutSeed, 7, 8) * 5);
    }
    if (hit.tick) B.phase += p == Wrap ? 1 : p == Raw ? 3 : 0;
    if (hit.rat) {
        B.sinceRat = 0; B.rj++;
        if (p == Wrap) B.stride += B.rj % 2 ? 1 : 2;
        if (p == Tear) B.roll += B.rj % 2 ? 3 : -5;
        if (p == Mosh) B.vecSeed++;
        if (p == Raw) { const int rs[] = {79, 81, 83, 53, 160}; B.rawStride = std::max(8, g_bg.w + rs[B.rj % 5] - 80); B.rawLeft = 2 + B.rj % 3; }
    }
    if (hit.metal) { B.sinceMetal = 0; B.blockSeed++; B.vecSeed += 7; }
}

int wayFor(int bar) {
    if (g_set.way != All) return g_set.way;
    return (bar / 2) % 5;
}

// ---------------------------------------------------------------- applying specs to windows

void clearShards(bool held) {
    TDeskTop* d = TProgram::deskTop;
    if (d) d->lock();
    std::vector<TMoshShard*> all;
    for (auto& kv : g_pool) all.push_back(kv.second);
    if (held) all.insert(all.end(), g_held.begin(), g_held.end());
    for (TMoshShard* s : all) TObject::destroy(s);
    g_pool.clear();
    if (held) g_held.clear();
    g_blocks.clear();
    g_shown = Grid{};
    if (d) d->unlock();
}

void apply(std::vector<Spec>& specs) {
    TDeskTop* d = TProgram::deskTop;
    if (!d) return;
    if ((int)specs.size() > g_set.count) specs.resize((size_t)g_set.count);
    d->lock();
    std::map<int, bool> want;
    for (Spec& s : specs) {
        want[s.slot] = true;
        if (s.keep) continue;   // stale on purpose
        TRect r = s.r;
        r.move(g_region.a.x, g_region.a.y);
        // the person's caret stays visible: a shard over it steps off the desk
        if (r.contains(g_caret)) r.move(0, 10000);
        auto it = g_pool.find(s.slot);
        TMoshShard* w = it == g_pool.end() ? nullptr : it->second;
        if (w && w->framed() != s.framed) {
            if (g_busy) continue;
            TObject::destroy(w);
            w = nullptr;
        }
        if (!w) {
            if (g_busy) continue;
            w = new TMoshShard(r, s.framed, s.title, s.slot);
            d->insert(w);
            g_pool[s.slot] = w;
        } else if (w->getBounds() != r) {
            w->locate(r);
        }
        w->setCells(std::move(s.cells));
    }
    if (!g_busy) {
        std::vector<TMoshShard*> gone;
        for (auto& kv : g_pool) if (!want.count(kv.first)) gone.push_back(kv.second);
        for (TMoshShard* s : gone) TObject::destroy(s);
        // restack: on snares a few shards come to the front
        if (B.sinceSnare == 0 && !g_pool.empty()) {
            int k = 1 + (int)(amt() * 6);
            for (int i = 0; i < k; ++i) {
                auto it = g_pool.begin();
                std::advance(it, (long)((size_t)(h3((int)g_step, i, 17) * g_pool.size()) % g_pool.size()));
                it->second->makeFirst();
            }
        }
    }
    d->unlock();
}

std::string idOfWin(TWindow* w) { return resident::hooks().idOf ? resident::hooks().idOf(w) : ""; }

TRect regionRect() {
    TDeskTop* d = TProgram::deskTop;
    TRect all = d ? d->getExtent() : TRect(0, 0, 0, 0);
    if (g_set.scope.empty()) return all;
    TWindow* w = resident::hooks().findWindow ? resident::hooks().findWindow(g_set.scope) : nullptr;
    if (!w || isShard(w)) return all;
    TRect r = w->getBounds();
    r.intersect(all);
    return r;
}

Grid crop(const Grid& t, const TRect& r) {
    Grid g;
    g.w = std::max(1, r.b.x - r.a.x); g.h = std::max(1, r.b.y - r.a.y);
    g.c.resize((size_t)g.w * g.h);
    for (int y = 0; y < g.h; ++y) for (int x = 0; x < g.w; ++x) g.c[(size_t)y * g.w + x] = t.at(r.a.x + x, r.a.y + y);
    return g;
}

void runStep(const Hit& hit) {
    auto t0 = Clock::now();
    if (g_screenOn && g_stepShown.w) g_prevShown = g_stepShown;   // the screen layer's flow moves on per step
    if (!g_on) {
        int pos = hit.step >= 0 ? hit.step : (B.pos + 1) % 16;
        B.active = wayFor(B.bar + (pos <= B.pos ? 1 : 0));
        advance(hit);
        g_step++;
        return;
    }
    int pos = hit.step >= 0 ? hit.step : (B.pos + 1) % 16;
    int w = wayFor(B.bar + (pos <= B.pos ? 1 : 0));
    if (w != B.active) {
        clearShards(false);
        restoreReal();
        B.active = w;
        B.stride = 0; B.phase = 0; B.zoom = 1; B.roll = 0; B.scroll = 0; B.rawLeft = 0;
    }
    advance(hit);
    g_step++;
    g_region = regionRect();
    TPoint c = caretPoint();
    g_caret = c;
    // the real windows themselves: resized to the wrong width (wrap), recoloured (bleed)
    if (g_set.scope.empty()) {
        if (w == Wrap && hit.kick && amt() > 0.2) {
            int i = 0;
            for (TWindow* rw : realWindows()) {
                if (!(rw->flags & wfGrow) || dynamic_cast<TDialog*>(rw)) { ++i; continue; }
                TRect base = g_real.count(rw) && g_real[rw].moved ? g_real[rw].orig : rw->getBounds();
                int dw = WRAPS[(B.nk + i * 3 + g_set.seed) % NWRAPS];
                dw = std::max(-(base.b.x - base.a.x) / 2, std::min(dw, 24));
                TRect r = base;
                r.b.x = std::max(r.a.x + 12, r.b.x + dw);
                resizeReal(rw, r);
                ++i;
            }
        }
        if (w == Bleed && hit.snare) {
            const int fam[] = {1, 3, 7, 0, 5, 4, 2, 6};
            int i = 0;
            for (TWindow* rw : realWindows()) {
                if (h3(i, B.stutSeed, 21) < 0.5 * amt()) colourReal(rw, fam[(B.rot + i) % 8]);
                ++i;
            }
        }
    }
    Grid t = crop(trueDesk(), g_region);
    std::vector<Spec> specs;
    switch (w) {
    case Wrap: wayWrap(t, specs); break;
    case Tear: wayTear(t, specs); break;
    case Mosh: wayMosh(t, specs); break;
    case Bleed: wayBleed(t, specs); break;
    default: wayRaw(t, specs); break;
    }
    apply(specs);
    g_stepMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    g_stepMsAvg = g_stepMsAvg == 0 ? g_stepMs : g_stepMsAvg * 0.9 + g_stepMs * 0.1;
}

// The internal pattern, after the film's drum parts: kicks with pushes, claps on 4 and
// 12, hats on the offbeats, a ratchet at the end of the bar, a crash every fourth bar.
Hit internalHit(int pos, int bar) {
    Hit h;
    h.step = pos;
    h.kick = pos == 0 || pos == 3 || pos == 6 || pos == 8 || pos == 11 || (pos == 14 && bar % 2);
    h.snare = pos == 4 || pos == 12;
    h.tick = pos % 2 == 1;
    h.rat = pos == 15 || (pos == 7 && bar % 4 == 3);
    h.metal = pos == 0 && bar % 4 == 0;
    h.accent = pos == 0;
    return h;
}

double stepMs() { return g_set.tempo > 0 ? 60000.0 / g_set.tempo / 4.0 : 0; }

std::string q(const std::string& s) {
    std::string o = "\"";
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o + "\"";
}
std::string str(const KV& kv, const char* k, const std::string& d = "") {
    auto it = kv.find(k);
    return it == kv.end() ? d : it->second;
}

void turnOn() {
    if (g_on) return;
    g_on = true;
    resident::Who w = resident::currentWho();
    g_by = w.actor;
    g_step = 0;
    B = Beat{};
    B.active = wayFor(0);
    g_bg = captureScreenDesk();
    g_nextAt = Clock::now();
    g_queue.clear();
    g_stepMsAvg = 0; g_loopMsAvg = 0;
    resident::addAct(w, std::string("mosh on (way ") + kWayNames[g_set.way] + ", amount " + std::to_string(g_set.amount) +
                     ", seed " + std::to_string(g_set.seed) + (g_set.scope.empty() ? "" : ", scope " + g_set.scope) + ")", "", "");
}

void screenStop() {
    if (!g_screenOn) return;
    g_screenOn = false;
    screenAfterImpl();
    screenRefreshAll();
}

void turnOff(const char* how) {
    bool screen = g_screenOn;
    screenStop();
    if (!g_on) {
        if (screen) resident::addAct(resident::currentWho(), std::string("mosh screen ") + how, "", "");
        return;
    }
    g_on = false;
    clearShards(true);
    restoreReal();
    g_queue.clear();
    if (TProgram::deskTop) TProgram::deskTop->redraw();
    resident::addAct(resident::currentWho(), std::string("mosh ") + how + " after " + std::to_string(g_step) +
                     " steps (on by " + g_by + ")", "", "");
}

std::string statusJson() {
    std::ostringstream o;
    o << "{\"on\":" << (g_on ? "true" : "false") << ",\"way\":" << q(kWayNames[g_set.way])
      << ",\"showing\":" << q(g_on ? kWayNames[B.active] : "") << ",\"amount\":" << g_set.amount
      << ",\"tempo\":" << g_set.tempo << ",\"seed\":" << g_set.seed << ",\"count\":" << g_set.count
      << ",\"scope\":" << q(g_set.scope.empty() ? "desktop" : g_set.scope) << ",\"shards\":" << g_pool.size()
      << ",\"held\":" << g_held.size() << ",\"screen\":" << (g_screenOn ? "true" : "false") << ",\"step\":" << g_step << ",\"bar\":" << B.bar
      << ",\"clock\":" << q(Clock::now() - g_last808 < std::chrono::seconds(1) ? "tr808" : g_set.tempo > 0 ? "internal" : "manual")
      << ",\"opened\":" << g_opened << ",\"closed\":" << g_closed << ",\"by\":" << q(g_by)
      << ",\"step_ms\":" << (int)(g_stepMs * 10) / 10.0 << ",\"step_ms_avg\":" << (int)(g_stepMsAvg * 10) / 10.0
      << ",\"loop_ms_avg\":" << (int)(g_loopMsAvg * 10) / 10.0 << "}";
    return o.str();
}

bool parseHits(const std::string& s, Hit& h) {
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (tok == "kick" || tok == "bd") h.kick = true;
        else if (tok == "snare" || tok == "sd" || tok == "cp") h.snare = true;
        else if (tok == "tick" || tok == "hat" || tok == "ch" || tok == "oh") h.tick = true;
        else if (tok == "rat" || tok == "ratchet") h.rat = true;
        else if (tok == "metal" || tok == "cy") h.metal = true;
        else if (tok == "accent") h.accent = true;
        else if (tok == "none" || tok.empty()) {}
        else return false;
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------- public

bool isShard(const TView* v) { return dynamic_cast<const TMoshShard*>(v) != nullptr; }
bool on() { return g_on || g_screenOn; }
uint64_t stepCount() { return g_step; }
uint64_t openedTotal() { return g_opened; }
uint64_t closedTotal() { return g_closed; }

std::string summaryJson() {
    std::ostringstream o;
    o << "{\"on\":" << (g_on || g_screenOn ? "true" : "false") << ",\"way\":" << q(g_on || g_screenOn ? kWayNames[B.active] : "")
      << ",\"screen\":" << (g_screenOn ? "true" : "false") << ",\"shards\":" << (g_pool.size() + g_held.size()) << ",\"step\":" << g_step << ",\"opened\":" << g_opened
      << ",\"closed\":" << g_closed << "}";
    return o.str();
}

void screenBefore() {
    if (g_screenDepth++ != 0 || !g_screenOn || TheTopView || !TScreen::screenBuffer) return;
    Grid S = screenGrid();
    if (S.c.empty()) return;
    Grid N = screenCompose(S);
    int W = S.w, top = screenTop();
    for (int y = 0; y < S.h; ++y) {
        bool row = false;
        for (int x = 0; x < W; ++x) {
            size_t k = (size_t)y * W + x;
            if (N.c[k] == S.c[k]) continue;
            size_t i = (size_t)(top + y) * W + x;
            g_saved.emplace_back(i, S.c[k]);
            g_wrote.push_back(N.c[k]);
            TScreen::screenBuffer[i] = N.c[k];
            row = true;
        }
        if (row) THardwareInfo::screenWrite(0, top + y, TScreen::screenBuffer + (size_t)(top + y) * W, W);
    }
    g_stepShown = std::move(N);
}

void screenAfter() {
    if (g_screenDepth > 0) --g_screenDepth;
    if (g_screenDepth == 0) screenAfterImpl();
}

void tick() {
    screenAfterImpl();   // idle work, IPC and look_study see the true screen
    auto now = Clock::now();
    if (g_on && g_lastTick.time_since_epoch().count()) {
        double gap = std::chrono::duration<double, std::milli>(now - g_lastTick).count();
        g_loopMsAvg = g_loopMsAvg == 0 ? gap : g_loopMsAvg * 0.95 + gap * 0.05;
    }
    g_lastTick = now;
    if (!g_on && !g_screenOn) return;
    if (TheTopView) return;   // a menu or dialog is open: the mosh waits
    bool from808 = now - g_last808 < std::chrono::seconds(1);
    if (!from808 && g_set.tempo > 0 && now >= g_nextAt) {
        int pos = (B.pos + 1) % 16;
        g_queue.push_back(internalHit(pos, B.bar + (pos == 0 ? 1 : 0)));
        g_nextAt += std::chrono::microseconds((long long)(stepMs() * 1000));
        if (now - g_nextAt > std::chrono::milliseconds(200)) g_nextAt = now;   // fell behind: resync
    }
    int n = 0;
    while (!g_queue.empty() && n < 4) {
        Hit h = g_queue.front();
        g_queue.pop_front();
        runStep(h);
        ++n;
    }
}

void noteEvent(const TEvent& ev) {
    if (!(g_on || g_screenOn) || ev.what != evKeyDown || ev.keyDown.keyCode != kbEsc) return;
    auto now = Clock::now();
    if (now - g_lastEsc < std::chrono::milliseconds(800)) {
        resident::WhoScope ws(resident::eventWho());
        turnOff("stopped by Esc Esc");
        g_lastEsc = {};
        return;
    }
    g_lastEsc = now;
}

void noteTr808(const char* type, const std::string& body) {
    if (!(g_on || g_screenOn) || std::strcmp(type, "tr808_step") != 0) return;
    g_last808 = Clock::now();
    Hit h;
    size_t p = body.find("\"step\":");
    if (p != std::string::npos) h.step = std::atoi(body.c_str() + p + 7) % 16;
    size_t v0 = body.find("\"voices\":[");
    size_t v1 = v0 == std::string::npos ? v0 : body.find(']', v0);
    std::string vs = v0 == std::string::npos ? "" : body.substr(v0, v1 - v0);
    auto has = [&](const char* id) { return vs.find(std::string("\"") + id + "\"") != std::string::npos; };
    h.kick = has("bd");
    h.snare = has("sd") || has("cp");
    h.tick = has("ch") || has("oh");
    h.rat = has("rs") || has("cb") || has("cl") || has("lt") || has("mt") || has("ht");
    h.metal = has("cy") || has("ma");
    h.accent = body.find("\"accent\":true") != std::string::npos;
    if (g_queue.size() < 8) g_queue.push_back(h);
}

bool isCommand(const std::string& name) { return name == "mosh" || name.rfind("mosh_", 0) == 0; }

std::string command(const std::string& nameIn, const KV& kvIn) {
    std::string name = nameIn;
    KV kv = kvIn;
    if (name == "mosh") {
        // One command with words, for the chat and Scramble: "on", "way tear", "amount 40", ...
        std::string args = str(kv, "args", str(kv, "text"));
        std::stringstream ss(args);
        std::string verb, val;
        ss >> verb >> val;
        if (verb.empty()) return statusJson();
        name = "mosh_" + verb;
        if (verb == "way" || verb == "on") { if (!val.empty()) kv["way"] = val; }
        else if (verb == "amount" || verb == "count") kv["value"] = val;
        else if (verb == "tempo") kv["bpm"] = val;
        else if (verb == "seed") kv["n"] = val;
        else if (verb == "scope") { kv["scope"] = val; std::string id; ss >> id; if (!id.empty()) kv["id"] = id; }
        else if (verb == "step") kv["hits"] = val;
        else if (verb == "screen") kv["value"] = val.empty() ? "on" : val;
    }
    if (name == "mosh_status") return statusJson();
    if (name == "mosh_screen") {
        std::string v = str(kv, "value", str(kv, "on", "on"));
        bool want = v == "on" || v == "1" || v == "true";
        if (want && !g_screenOn) {
            if (!g_on) { B = Beat{}; B.active = wayFor(0); g_step = 0; g_nextAt = Clock::now(); }
            g_prevShown = Grid{}; g_stepShown = Grid{};
            g_screenOn = true;
            resident::addAct(resident::currentWho(), "mosh screen on", "", "");
        } else if (!want && g_screenOn) {
            g_screenOn = false;
            screenAfterImpl();
            screenRefreshAll();
            resident::addAct(resident::currentWho(), "mosh screen off", "", "");
        }
        return statusJson();
    }
    if (name == "mosh_on") {
        if (kv.count("way")) { std::string w = kv["way"]; for (int i = 0; i < 6; ++i) if (w == kWayNames[i]) g_set.way = (Way)i; }
        turnOn();
        return statusJson();
    }
    if (name == "mosh_off") { turnOff("off"); return statusJson(); }
    if (name == "mosh_panic") { turnOff("panic"); return statusJson(); }
    if (name == "mosh_toggle") { if (g_on) turnOff("off"); else turnOn(); return statusJson(); }
    if (name == "mosh_way") {
        std::string w = str(kv, "way");
        for (int i = 0; i < 6; ++i) if (w == kWayNames[i]) { g_set.way = (Way)i; return statusJson(); }
        return "err way must be wrap|tear|mosh|bleed|raw|all";
    }
    if (name == "mosh_amount") {
        int v = std::atoi(str(kv, "value", str(kv, "amount", "-1")).c_str());
        if (v < 0 || v > 100) return "err amount 0..100";
        g_set.amount = v;
        return statusJson();
    }
    if (name == "mosh_tempo") {
        int v = std::atoi(str(kv, "bpm", str(kv, "value", "-1")).c_str());
        if (v != 0 && (v < 20 || v > 400)) return "err tempo 0 (manual) or 20..400";
        g_set.tempo = v;
        g_nextAt = Clock::now();
        return statusJson();
    }
    if (name == "mosh_seed") {
        std::string v = str(kv, "n", str(kv, "value"));
        if (v.empty()) return "err missing n";
        g_set.seed = (uint32_t) std::strtoul(v.c_str(), nullptr, 10);
        return statusJson();
    }
    if (name == "mosh_count") {
        int v = std::atoi(str(kv, "value", str(kv, "n", "-1")).c_str());
        if (v < 1 || v > 4000) return "err count 1..4000";
        g_set.count = v;
        return statusJson();
    }
    if (name == "mosh_scope") {
        std::string s = str(kv, "scope", "desktop");
        if (s == "desktop") g_set.scope.clear();
        else if (s == "window") {
            std::string id = str(kv, "id");
            if (id.empty()) return "err scope window needs id";
            TWindow* w = resident::hooks().findWindow ? resident::hooks().findWindow(id) : nullptr;
            if (!w || isShard(w)) return "err window not found: " + id;
            g_set.scope = id;
        } else return "err scope desktop|window";
        if (g_on) { clearShards(false); restoreReal(); }
        return statusJson();
    }
    if (name == "mosh_step") {
        if (!g_on && !g_screenOn) return "err mosh is off";
        Hit h;
        if (!parseHits(str(kv, "hits"), h)) return "err hits: kick,snare,tick,rat,metal,accent";
        int n = std::max(1, std::min(256, std::atoi(str(kv, "n", "1").c_str())));
        for (int i = 0; i < n; ++i) runStep(h);
        return statusJson();
    }
    return "err unknown mosh command";
}

bool handleCommand(unsigned short cmd) {
    if (cmd == cmMoshToggle) { if (g_on) turnOff("off"); else turnOn(); return true; }
    if (cmd == cmMoshPanic) { turnOff("panic"); return true; }
    return false;
}

}  // namespace deskmosh
