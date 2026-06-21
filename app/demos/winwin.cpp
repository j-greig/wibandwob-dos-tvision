// WINWIN.CPP — homage to Andreas Gysin's "Win-Win" text-mode window manager.
//
// THE POINT: this answers Gysin's "depends if it has a frame buffer". Instead of
// moving TWindow objects (see demo_meltdown for that retained-mode approach),
// this uses ONE full-screen TView as a raw CELL FRAMEBUFFER — the play.core /
// immediate-mode paradigm, hosted inside Turbo Vision. Windows are DATA
// rasterised into the buffer each frame; then a per-column MELT post-pass drips
// the whole composited image downward — an effect impossible with window motion
// alone (it reads & rewrites arbitrary cells). TV's damage/diff still flushes
// only the cells that changed, so a full-frame redraw stays cheap.
//
// Breathing loop: grid -> subdivide(dense) -> cascade/mirror -> MELT -> collapse.
// Keys: g grid  s subdivide  c cascade  m mirror  . / , melt+/-  space freeze
//       a auto-breathe  +/- window count  Esc/Alt-X quit
//
// Built by Wib & Wob for Zilla, 2026-06-21.   wib&wob

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>

#define Uses_TApplication
#define Uses_TDeskTop
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TRect
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TSubMenu
#define Uses_TDrawBuffer
#define Uses_TView

#include <tvision/tv.h>

// BIOS attribute bytes (hi nibble bg 0-7, lo nibble fg 0-15)
static const uchar A_DESK   = 0x08; // dark-gray on black (hatched desktop)
static const uchar A_FILL   = 0x44; // red on red (solid body)
static const uchar A_BORDER = 0x4E; // yellow on red
static const uchar A_TITLE  = 0x4E; // yellow on red
static const uchar A_SHADOW = 0x00; // black on black

enum Mode { M_GRID, M_SUBDIV, M_CASCADE, M_MIRROR };

struct Win {
    double cx, cy, cw, ch;    // current (eased)
    double tx, ty, tw, th;    // target
    int id;
};

static inline double hashx(int x) {
    unsigned h = (unsigned)x * 2654435761u;
    return ((h >> 16) & 0xff) / 255.0;
}

class TWinWin : public TView {
    int W = 0, H = 0;
    std::vector<char>  ch;
    std::vector<uchar> at;
    std::vector<Win> wins;
    Mode mode = M_GRID;
    double clock = 0, melt = 0, autoT = 0;
    bool autoBreathe = false, frozen = false;
    int nextId = 1, active = 12, subdivLevel = 1;
    double drawMs = 0;   // EMA of compose+blit cost per frame
    std::chrono::steady_clock::time_point last;

public:
    enum { cmGrid = 300, cmSubdiv, cmCascade, cmMirror, cmAuto,
           cmFreeze, cmMore, cmLess, cmMeltUp, cmMeltDn };

    TWinWin(const TRect& r) : TView(r) {
        growMode = gfGrowHiX | gfGrowHiY;
        options |= ofSelectable;          // so it can become `current` for keys
        eventMask |= evKeyDown;
        last = std::chrono::steady_clock::now();
        ensureSize();
        for (int i = 0; i < 600; ++i) spawn();   // big pool for stress levels
        buildTargets(); snap();
    }

    void ensureSize() {
        if (size.x != W || size.y != H) {
            W = size.x; H = size.y;
            ch.assign((size_t)W * H, ' ');
            at.assign((size_t)W * H, A_DESK);
        }
    }
    inline void put(int x, int y, char c, uchar a) {
        if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) {
            ch[(size_t)y * W + x] = c; at[(size_t)y * W + x] = a;
        }
    }
    void spawn() { Win w{}; w.id = nextId++; w.cx = W/2.0; w.cy = H/2.0; w.cw = w.ch = 2; wins.push_back(w); }

    // ---- layout targets ----------------------------------------------------
    void buildTargets() {
        int n = active;
        switch (mode) {
        case M_GRID: {
            int cols = 4, rows = (n + cols - 1) / cols;
            int gw = (W - 2) / cols, gh = (H - 2) / rows;
            for (int i = 0; i < n; ++i) {
                int c = i % cols, r = i / cols;
                set(i, 1 + c*gw, 1 + r*gh, gw - 2, gh - 1);
            }
            break;
        }
        case M_SUBDIV: {
            // progressive: each `s` press climbs subdivLevel -> more, smaller cells.
            // active is DERIVED here to exactly tile the screen at this depth.
            int cols = 4 * subdivLevel;
            int gw = std::max(6, (W - 1) / cols);
            int gh = std::max(3, gw / 2);                 // text cells are ~2:1 tall
            int rows = std::max(1, (H - 1) / gh);
            active = std::min((int)wins.size(), cols * rows);
            for (int i = 0; i < active; ++i) {
                int c = i % cols, r = i / cols;
                set(i, 1 + c*gw, r*gh, gw - 1, gh - 1);
            }
            break;
        }
        case M_CASCADE: {
            // scale window size down as count rises; wrap positions to fill screen
            double scl = std::sqrt(12.0 / std::max(n, 12));
            int ww = std::max(8, (int)(W * 0.30 * scl));
            int wh = std::max(4, (int)(H * 0.40 * scl));
            int sx = 3, sy = 2;
            int spanX = std::max(1, W - ww - 1), spanY = std::max(1, H - wh - 1);
            for (int i = 0; i < n; ++i)
                set(i, 1 + (i*sx) % spanX, (i*sy) % spanY, ww, wh);
            break;
        }
        case M_MIRROR: {
            int sx = 3, sy = 2, ww = W*0.30, wh = H*0.36;
            int maxk = std::max(1, (H - (int)wh) / sy);
            for (int i = 0; i < n; ++i) {
                int side = i & 1, k = (i / 2) % maxk;
                int x = side ? (W - (int)ww - 1 - k*sx) : (1 + k*sx);
                set(i, x, k*sy, ww, wh);
            }
            break;
        }
        }
    }
    void set(int i, int x, int y, int w, int h) {
        if (i >= (int)wins.size()) return;
        wins[i].tx = x; wins[i].ty = y;
        wins[i].tw = std::max(6, w); wins[i].th = std::max(3, h);
    }
    void snap() { for (auto& w : wins) { w.cx=w.tx; w.cy=w.ty; w.cw=w.tw; w.ch=w.th; } }

    // ---- rasterise one window into the framebuffer -------------------------
    void drawWin(const Win& w) {
        int x = (int)std::lround(w.cx), y = (int)std::lround(w.cy);
        int ww = (int)std::lround(w.cw), wh = (int)std::lround(w.ch);
        if (ww < 4 || wh < 2) return;
        // shadow (offset +2,+1) painted first; body overwrites the overlap
        for (int yy = y+1; yy <= y+wh; ++yy)
            for (int xx = x+2; xx <= x+ww+1; ++xx) put(xx, yy, ' ', A_SHADOW);
        // body fill
        for (int yy = y; yy <= y+wh; ++yy)
            for (int xx = x; xx <= x+ww; ++xx) put(xx, yy, ' ', A_FILL);
        // border
        for (int xx = x+1; xx < x+ww; ++xx) { put(xx, y, '-', A_BORDER); put(xx, y+wh, '-', A_BORDER); }
        for (int yy = y+1; yy < y+wh; ++yy) { put(x, yy, '|', A_BORDER); put(x+ww, yy, '|', A_BORDER); }
        put(x, y, '+', A_BORDER); put(x+ww, y, '+', A_BORDER);
        put(x, y+wh, '+', A_BORDER); put(x+ww, y+wh, '+', A_BORDER);
        // title: WN:x,y  (Gysin's live coordinate label)
        char t[32];
        std::snprintf(t, sizeof(t), " W%d:%d,%d ", w.id, x, y);
        int tl = (int)std::strlen(t);
        for (int k = 0; k < tl && x+2+k < x+ww; ++k) put(x+2+k, y, t[k], A_TITLE);
    }

    // ---- per-column melt: drip the composited image downward ---------------
    void meltPass() {
        int m = (int)melt;
        if (m <= 0) return;
        for (int x = 0; x < W; ++x) {
            int off = (int)(m * (0.45 + 0.55 * hashx(x)));
            if (off <= 0) continue;
            for (int y = H - 1; y >= 0; --y) {
                int src = y - off;
                size_t d = (size_t)y * W + x;
                if (src >= 0) { ch[d] = ch[(size_t)src*W + x]; at[d] = at[(size_t)src*W + x]; }
                else { ch[d] = ' '; at[d] = A_DESK; }
            }
        }
    }

    void compose() {
        ensureSize();
        std::fill(ch.begin(), ch.end(), ' ');
        std::fill(at.begin(), at.end(), A_DESK);
        for (int i = 0; i < active && i < (int)wins.size(); ++i) drawWin(wins[i]);
        meltPass();
    }

    void draw() override {
        TDrawBuffer b;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x)
                b.moveChar(x, ch[(size_t)y*W + x], TColorAttr((int)at[(size_t)y*W + x]), 1);
            writeLine(0, y, (short)W, 1, b);
        }
        // live cost meter overlay (always visible, survives melt)
        char st[96];
        int fps = drawMs > 0.01 ? (int)std::min(40.0, 1000.0 / drawMs) : 40;
        std::snprintf(st, sizeof(st), " Win-Win  windows=%d  draw=%.2fms  ~%dfps (cap40) ",
                      active, drawMs, fps);
        TDrawBuffer sb;
        sb.moveChar(0, ' ', TColorAttr(0x1F), (short)W);
        sb.moveStr(0, st, TColorAttr(0x1F));
        writeLine(0, 0, (short)W, 1, sb);
    }

    void setMode(Mode mm) { mode = mm; melt = 0; buildTargets(); }

    void step(double dt) {
        clock += dt;
        if (autoBreathe) {
            autoT += dt;
            // breath: grid(0-3) -> subdiv(3-6) -> mirror+melt(6-10) -> loop
            double p = std::fmod(autoT, 10.0);
            if      (p < 3 && mode != M_GRID)    setMode(M_GRID);
            else if (p >= 3 && p < 6 && mode != M_SUBDIV) { subdivLevel = 2; setMode(M_SUBDIV); }
            else if (p >= 6 && mode != M_MIRROR) { active = 18; setMode(M_MIRROR); }
            if (p >= 7) melt = (p - 7) * 9.0;      // ramp the drip in the down-phase
        }
        for (int i = 0; i < (int)wins.size(); ++i) {
            Win& w = wins[i];
            double a = 1.0 - std::exp(-7.0 * dt);
            w.cx += (w.tx - w.cx) * a; w.cy += (w.ty - w.cy) * a;
            w.cw += (w.tw - w.cw) * a; w.ch += (w.th - w.ch) * a;
        }
        auto t0 = std::chrono::steady_clock::now();
        compose();
        drawView();
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;
        drawMs = drawMs <= 0 ? ms : drawMs * 0.9 + ms * 0.1;
    }

    void handleEvent(TEvent& e) override {
        TView::handleEvent(e);
        if (e.what == evKeyDown) {
            switch (e.keyDown.charScan.charCode) {
            case 'g': active=12; setMode(M_GRID);    clearEvent(e); break;
            case 's':
                // each press subdivides finer; wrap to level 1 when cells hit the floor
                if (mode == M_SUBDIV) {
                    subdivLevel++;
                    if (4 * subdivLevel > (W - 1) / 6) subdivLevel = 1;
                } else subdivLevel = 1;
                setMode(M_SUBDIV);  clearEvent(e); break;
            case 'c': active=14; setMode(M_CASCADE); clearEvent(e); break;
            case 'm': active=18; setMode(M_MIRROR);  clearEvent(e); break;
            case 'a': autoBreathe = !autoBreathe; autoT = 0; clearEvent(e); break;
            case ' ': frozen = !frozen; clearEvent(e); break;
            case '.': melt += 4; clearEvent(e); break;
            case ',': melt = std::max(0.0, melt - 4); clearEvent(e); break;
            case '+': case '=': active = std::min((int)wins.size(), active+2); buildTargets(); clearEvent(e); break;
            case '-': case '_': active = std::max(2, active-2); buildTargets(); clearEvent(e); break;
            // stress levels (cascade): 10 / 30 / 60 / 100 / 200 / 400
            case '1': active=10;  setMode(M_CASCADE); clearEvent(e); break;
            case '2': active=30;  setMode(M_CASCADE); clearEvent(e); break;
            case '3': active=60;  setMode(M_CASCADE); clearEvent(e); break;
            case '4': active=100; setMode(M_CASCADE); clearEvent(e); break;
            case '5': active=200; setMode(M_CASCADE); clearEvent(e); break;
            case '6': active=400; setMode(M_CASCADE); clearEvent(e); break;
            }
        }
    }

    void idle() {
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration_cast<std::chrono::microseconds>(now - last).count()/1e6;
        if (dt < 1.0/40.0) return;
        last = now;
        if (!frozen) step(dt);
    }
};

class TWinWinApp : public TApplication {
    TWinWin* view;
public:
    TWinWinApp() : TProgInit(&TWinWinApp::initStatusLine,
                             &TWinWinApp::initMenuBar,
                             &TApplication::initDeskTop) {
        view = new TWinWin(deskTop->getExtent());
        deskTop->insert(view);
        view->makeFirst();
        deskTop->setCurrent(view, TView::normalSelect);   // key events go to `current`
    }
    static TMenuBar* initMenuBar(TRect r) {
        r.b.y = r.a.y + 1;
        return new TMenuBar(r, *new TSubMenu("~W~in-Win", kbAltW) +
            *new TMenuItem("E~x~it", cmQuit, kbAltX));
    }
    static TStatusLine* initStatusLine(TRect r) {
        r.a.y = r.b.y - 1;
        return new TStatusLine(r, *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~g~rid ~s~ubdiv ~c~ascade ~m~irror  .,melt  ~Space~ freeze  ~a~uto", 0, 0) +
            *new TStatusItem("~Alt-X~", kbAltX, cmQuit));
    }
    void idle() override { TApplication::idle(); if (view) view->idle(); }
};

int main() { TWinWinApp app; app.run(); return 0; }
