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
// Keys: g grid  s subdivide(press repeatedly = finer)  c cascade  m mirror
//       c/m KEEP the current window count   . start/accelerate pour  , ease off
//       space freeze  a autoplay (choreographed scene loop)  +/- window count
//       1-6 cascade stress (10..400)  Esc/Alt-X quit
// Colour drifts per-window over time; melt has gravity (lower cells fall faster).
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
    double clock = 0, melt = 0, meltVel = 0, autoT = 0, sceneT = 0, sceneDur = 2.5;
    bool autoBreathe = false, frozen = false;
    int nextId = 1, active = 12, subdivLevel = 1, scene = 0, sceneCount = 1;
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
        // each window owns a background colour that SLOWLY DRIFTS over time, so the
        // whole field shimmers through the palette instead of sitting flat red.
        static const uchar PAL[6] = { 1, 2, 3, 5, 6, 4 };   // blue grn cyan mag brn red (bg)
        uchar bg     = PAL[((unsigned)(w.id + (int)(clock * 0.7))) % 6];
        uchar fill   = (uchar)((bg << 4) | 0x0F);           // bright white body
        uchar border = (uchar)((bg << 4) | 0x0E);           // yellow frame
        // shadow (offset +2,+1) painted first; body overwrites the overlap
        for (int yy = y+1; yy <= y+wh; ++yy)
            for (int xx = x+2; xx <= x+ww+1; ++xx) put(xx, yy, ' ', A_SHADOW);
        // body fill
        for (int yy = y; yy <= y+wh; ++yy)
            for (int xx = x; xx <= x+ww; ++xx) put(xx, yy, ' ', fill);
        // border
        for (int xx = x+1; xx < x+ww; ++xx) { put(xx, y, '-', border); put(xx, y+wh, '-', border); }
        for (int yy = y+1; yy < y+wh; ++yy) { put(x, yy, '|', border); put(x+ww, yy, '|', border); }
        put(x, y, '+', border); put(x+ww, y, '+', border);
        put(x, y+wh, '+', border); put(x+ww, y+wh, '+', border);
        // title: WN:x,y  (Gysin's live coordinate label)
        char t[32];
        std::snprintf(t, sizeof(t), " W%d:%d,%d ", w.id, x, y);
        int tl = (int)std::strlen(t);
        for (int k = 0; k < tl && x+2+k < x+ww; ++k) put(x+2+k, y, t[k], border);
    }

    // ---- per-column melt: drip the composited image downward ---------------
    // GRAVITY: drip grows with depth, so lower cells fall further than upper
    // ones and the image STRETCHES as it pours (acceleration, not a rigid slide).
    void meltPass() {
        if (melt <= 0) return;
        double Hm1 = std::max(1, H - 1);
        for (int x = 0; x < W; ++x) {
            double speed = 0.45 + 0.55 * hashx(x);   // per-column drip rate (stable)
            for (int y = H - 1; y >= 0; --y) {
                double depth = y / Hm1;               // 0 at top, 1 at bottom
                double fall  = 0.20 + 0.80 * depth * depth;  // ~depth^2 = accelerating
                int off = (int)(melt * speed * fall);
                int src = y - off;                    // off>=0, so src is always ABOVE y
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

    // ---- autoplay: a choreographed loop of scenes, not a 3-state breath -----
    // n means subdivLevel for SUBDIV scenes, else the window count. mv is the
    // melt velocity kicked in for that scene (0 = no drip).
    void applyScene() {
        static const struct { double dur; Mode mode; int n; double mv; } S[] = {
            { 2.6, M_GRID,    12,   0 },   // calm grid
            { 1.8, M_SUBDIV,   2,   0 },   // split
            { 1.8, M_SUBDIV,   4,   0 },   // split finer
            { 2.2, M_CASCADE, 30,   0 },   // fan out
            { 2.2, M_MIRROR,  24,   0 },   // symmetry
            { 2.8, M_MIRROR,  24,  14 },   // ...and pour
            { 1.6, M_CASCADE,140,   0 },   // swarm burst
            { 2.4, M_SUBDIV,   5,  18 },   // deep drip
            { 2.0, M_CASCADE, 60,   8 },   // cascade melt
            { 2.6, M_GRID,    12,   0 },   // heal back to calm
        };
        sceneCount = (int)(sizeof(S) / sizeof(S[0]));
        const auto& s = S[scene % sceneCount];
        sceneDur = s.dur;
        if (s.mode == M_SUBDIV) subdivLevel = s.n; else active = s.n;
        setMode(s.mode);          // resets melt to 0
        meltVel = s.mv;           // ...then arm the drip for this scene
    }

    void step(double dt) {
        clock += dt;
        if (autoBreathe) {
            sceneT += dt;
            if (sceneT >= sceneDur) { sceneT = 0; scene = (scene + 1) % sceneCount; applyScene(); }
        }
        // melt advances from its VELOCITY, so one tap of '.' keeps pouring on its
        // own; '.' adds drip, ',' bleeds it back off (negative vel = heal upward).
        melt += meltVel * dt;
        if (melt <= 0) { melt = 0; if (meltVel < 0) meltVel = 0; }
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
            case 'g': autoBreathe=false; active=12; setMode(M_GRID);    clearEvent(e); break;
            case 's':
                // each press subdivides finer; wrap to level 1 when cells hit the floor
                autoBreathe = false;
                if (mode == M_SUBDIV) {
                    subdivLevel++;
                    if (4 * subdivLevel > (W - 1) / 6) subdivLevel = 1;
                } else subdivLevel = 1;
                setMode(M_SUBDIV);  clearEvent(e); break;
            // c / m KEEP the current window count (subdivide to a swarm, then reshape it)
            case 'c': autoBreathe=false; setMode(M_CASCADE); clearEvent(e); break;
            case 'm': autoBreathe=false; setMode(M_MIRROR);  clearEvent(e); break;
            case 'a':
                autoBreathe = !autoBreathe;
                if (autoBreathe) { scene = 0; sceneT = 0; applyScene(); }
                else meltVel = 0;
                clearEvent(e); break;
            case ' ': frozen = !frozen; clearEvent(e); break;
            // one tap of '.' keeps pouring; tap again = faster; ',' eases / heals
            case '.': meltVel += 18; melt += 1; clearEvent(e); break;
            case ',': meltVel -= 18; melt = std::max(0.0, melt - 6); clearEvent(e); break;
            case '+': case '=': active = std::min((int)wins.size(), active+2); buildTargets(); clearEvent(e); break;
            case '-': case '_': active = std::max(2, active-2); buildTargets(); clearEvent(e); break;
            // stress levels (cascade): 10 / 30 / 60 / 100 / 200 / 400
            case '1': autoBreathe=false; active=10;  setMode(M_CASCADE); clearEvent(e); break;
            case '2': autoBreathe=false; active=30;  setMode(M_CASCADE); clearEvent(e); break;
            case '3': autoBreathe=false; active=60;  setMode(M_CASCADE); clearEvent(e); break;
            case '4': autoBreathe=false; active=100; setMode(M_CASCADE); clearEvent(e); break;
            case '5': autoBreathe=false; active=200; setMode(M_CASCADE); clearEvent(e); break;
            case '6': autoBreathe=false; active=400; setMode(M_CASCADE); clearEvent(e); break;
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
