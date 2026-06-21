// MELTDOWN.CPP — Turbo Vision homage to Andreas Gysin's "Meltdown" (window
// choreography). NOTE: built from his two story images, not his source (the
// verse.works piece is gated). His data model leaks through the labels: every
// window is "WN:x,y" — window N at column x, row y — arranged in a parametric
// grid that melts into a cascade.
//
// This version is PURE MOTION: empty numbered frames, no animated contents. The
// interest is entirely in how the frames move. Windows ease between named
// layouts; titles show live coordinates as they travel, the way his do.
//
// Layouts:   g grid    c cascade    m mirror-cascade    b bounce (free physics)
// Keys:      a auto-cycle g->c->m    +/- add/del    f freeze    Esc/Alt-X quit
//
// Built by Wib & Wob for Zilla, 2026-06-21.   wib&wob

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>

#define Uses_TApplication
#define Uses_TDeskTop
#define Uses_TEvent
#define Uses_TFrame
#define Uses_TGroup
#define Uses_TKeys
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TRect
#define Uses_TScreen
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TSubMenu
#define Uses_TView
#define Uses_TWindow

#include <tvision/tv.h>

// A window whose title we can rewrite every frame (live "WN:x,y" label).
class TLabeledWindow : public TWindow {
public:
    char label[40];
    TLabeledWindow(const TRect& r, const char* t, short num)
        : TWindowInit(&TWindow::initFrame), TWindow(r, t, num)
    {
        std::strncpy(label, t, sizeof(label) - 1);
        label[sizeof(label) - 1] = 0;
        flags &= ~(wfGrow | wfZoom | wfClose);   // move only; keep it simple
        growMode = 0;
    }
    const char* getTitle(short) override { return label; }
    void setLabel(const char* s) {
        if (std::strncmp(label, s, sizeof(label)) != 0) {
            std::strncpy(label, s, sizeof(label) - 1);
            label[sizeof(label) - 1] = 0;
            if (frame) frame->drawView();
        }
    }
};

enum Layout { L_GRID, L_CASCADE, L_MIRROR, L_BOUNCE };

struct Mover {
    TLabeledWindow* win;
    double x, y;        // current top-left (float)
    double tx, ty;      // target top-left (layout modes)
    double vx, vy;      // velocity (bounce mode)
    double k;           // per-window ease rate -> staggered arrival
    int w, h;
    int lastIX, lastIY; // last integer position placed
    int num;
};

class TMeltApp : public TApplication {
    std::vector<Mover> movers;
    std::chrono::steady_clock::time_point last;
    Layout mode = L_CASCADE;
    bool frozen = false;
    bool autoCycle = false;
    double autoTimer = 0;
    double clock = 0;
    int nextNum = 1;
    int winW = 18, winH = 6;

public:
    enum { cmAdd = 200, cmDel, cmGrid, cmCascade, cmMirror, cmBounce,
           cmFreeze, cmAuto };

    TMeltApp() : TProgInit(&TMeltApp::initStatusLine,
                           &TMeltApp::initMenuBar,
                           &TApplication::initDeskTop)
    {
        last = std::chrono::steady_clock::now();
        TRect dr = deskTop->getExtent();
        if (dr.b.x < 80) { winW = 14; winH = 5; }
        for (int i = 0; i < 12; ++i) spawn();
        relayout();
        snapToTargets();   // start already arranged so first frame is clean
    }

    static TMenuBar* initMenuBar(TRect r) {
        r.b.y = r.a.y + 1;
        return new TMenuBar(r, *new TSubMenu("~M~eltdown", kbAltM) +
            *new TMenuItem("~G~rid", cmGrid, 'g') +
            *new TMenuItem("~C~ascade", cmCascade, 'c') +
            *new TMenuItem("~M~irror cascade", cmMirror, 'm') +
            *new TMenuItem("~B~ounce", cmBounce, 'b') +
            newLine() +
            *new TMenuItem("~A~uto-cycle", cmAuto, 'a') +
            *new TMenuItem("~F~reeze", cmFreeze, 'f') +
            *new TMenuItem("~+~ add", cmAdd, kbGrayPlus) +
            *new TMenuItem("~-~ remove", cmDel, kbGrayMinus) +
            newLine() +
            *new TMenuItem("E~x~it", cmQuit, kbAltX));
    }

    static TStatusLine* initStatusLine(TRect r) {
        r.a.y = r.b.y - 1;
        return new TStatusLine(r, *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~G~rid", 0, cmGrid) +
            *new TStatusItem("~C~ascade", 0, cmCascade) +
            *new TStatusItem("~M~irror", 0, cmMirror) +
            *new TStatusItem("~B~ounce", 0, cmBounce) +
            *new TStatusItem("~A~uto", 0, cmAuto) +
            *new TStatusItem("~+/-~", kbGrayPlus, cmAdd) +
            *new TStatusItem("~Alt-X~ exit", kbAltX, cmQuit));
    }

    void spawn() {
        int num = nextNum++;
        char t[40];
        std::snprintf(t, sizeof(t), "W%d", num);
        TRect r(1, 0, 1 + winW, 0 + winH);
        TLabeledWindow* win = new TLabeledWindow(r, t, wnNoNumber);
        deskTop->insert(win);
        Mover m{};
        m.win = win; m.w = winW; m.h = winH; m.num = num;
        m.x = m.tx = 1; m.y = m.ty = 0;
        m.lastIX = -9999; m.lastIY = -9999;
        m.k = 5.0 + (rand() % 40) / 10.0;   // 5.0 .. 9.0
        double ang = (rand() % 628) / 100.0, sp = 5 + (rand() % 60) / 10.0;
        m.vx = std::cos(ang) * sp; m.vy = std::sin(ang) * sp;
        movers.push_back(m);
    }

    void removeOne() {
        if (movers.empty()) return;
        Mover m = movers.back(); movers.pop_back();
        deskTop->remove(m.win);
        TObject::destroy(m.win);
    }

    // --- layout target generators ------------------------------------------
    void relayout() {
        TRect dr = deskTop->getExtent();
        int W = dr.b.x, H = dr.b.y;
        int N = (int)movers.size();
        int gx = 2, gy = 1;
        int cols = std::max(1, (W - 1) / (winW + gx));

        for (int i = 0; i < N; ++i) {
            Mover& m = movers[i];
            switch (mode) {
            case L_GRID: {
                int col = i % cols, row = i / cols;
                m.tx = 1 + col * (winW + gx);
                m.ty = 0 + row * (winH + gy);
                break;
            }
            case L_CASCADE: {
                int stepX = 3, stepY = 2;
                int perRun = std::max(1, (H - winH) / stepY);
                int run = i / perRun, k = i % perRun;
                m.tx = 1 + k * stepX + run * (winW + 4);
                m.ty = 0 + k * stepY;
                break;
            }
            case L_MIRROR: {
                // Two interleaved staircases mirrored across the vertical axis;
                // they descend and meet toward the middle — the "meltdown".
                int stepX = 3, stepY = 2;
                int side = i & 1;              // 0 = from left, 1 = from right
                int k = i / 2;
                int maxK = std::max(1, (H - winH) / stepY);
                k %= maxK;
                if (!side) m.tx = 1 + k * stepX;
                else       m.tx = (W - winW - 1) - k * stepX;
                m.ty = 0 + k * stepY;
                break;
            }
            case L_BOUNCE:
                break;  // physics drives targets, not these
            }
        }
    }

    void snapToTargets() {
        for (auto& m : movers) { m.x = m.tx; m.y = m.ty; place(m, true); }
    }

    void place(Mover& m, bool force) {
        int ix = (int)std::lround(m.x), iy = (int)std::lround(m.y);
        if (force || ix != m.lastIX || iy != m.lastIY) {
            m.lastIX = ix; m.lastIY = iy;
            m.win->moveTo(ix, iy);
            char buf[40];
            std::snprintf(buf, sizeof(buf), "W%d:%d,%d", m.num, ix, iy);
            m.win->setLabel(buf);
        }
    }

    void handleEvent(TEvent& e) override {
        TApplication::handleEvent(e);
        if (e.what == evCommand) {
            switch (e.message.command) {
            case cmGrid:    mode = L_GRID;    relayout(); clearEvent(e); break;
            case cmCascade: mode = L_CASCADE; relayout(); clearEvent(e); break;
            case cmMirror:  mode = L_MIRROR;  relayout(); clearEvent(e); break;
            case cmBounce:  mode = L_BOUNCE;              clearEvent(e); break;
            case cmAuto:    autoCycle = !autoCycle; autoTimer = 0; clearEvent(e); break;
            case cmFreeze:  frozen = !frozen;            clearEvent(e); break;
            case cmAdd:     spawn(); relayout(); clearEvent(e); break;
            case cmDel:     removeOne(); relayout(); clearEvent(e); break;
            default: break;
            }
        }
    }

    void step(double dt) {
        clock += dt;
        if (autoCycle) {
            autoTimer += dt;
            if (autoTimer > 3.5) {
                autoTimer = 0;
                mode = (mode == L_GRID) ? L_CASCADE
                     : (mode == L_CASCADE) ? L_MIRROR : L_GRID;
                relayout();
            }
        }

        if (mode == L_BOUNCE) { bounce(dt); return; }

        // Ease toward targets with a gentle breathing sway so frames stay alive
        // (motion complexity without animating any contents).
        for (size_t i = 0; i < movers.size(); ++i) {
            Mover& m = movers[i];
            double swayX = std::sin(clock * 1.3 + i * 0.7) * 0.9;
            double swayY = std::cos(clock * 1.1 + i * 0.5) * 0.5;
            double gx = m.tx + swayX, gy = m.ty + swayY;
            double a = 1.0 - std::exp(-m.k * dt);
            m.x += (gx - m.x) * a;
            m.y += (gy - m.y) * a;
            place(m, false);
        }
    }

    void bounce(double dt) {
        TRect dr = deskTop->getExtent();
        double maxX = dr.b.x, maxY = dr.b.y;
        for (auto& m : movers) {
            m.x += m.vx * dt; m.y += m.vy * dt;
            if (m.x < 0)            { m.x = 0; m.vx = std::fabs(m.vx); }
            if (m.x + m.w > maxX)   { m.x = maxX - m.w; m.vx = -std::fabs(m.vx); }
            if (m.y < 0)            { m.y = 0; m.vy = std::fabs(m.vy); }
            if (m.y + m.h > maxY)   { m.y = maxY - m.h; m.vy = -std::fabs(m.vy); }
        }
        // soft repulsion on overlap (windows influencing windows)
        for (size_t i = 0; i < movers.size(); ++i)
            for (size_t j = i + 1; j < movers.size(); ++j) {
                Mover& a = movers[i]; Mover& b = movers[j];
                double dx = (b.x + b.w/2.0) - (a.x + a.w/2.0);
                double dy = (b.y + b.h/2.0) - (a.y + a.h/2.0);
                double ox = (a.w + b.w)/2.0 - std::fabs(dx);
                double oy = (a.h + b.h)/2.0 - std::fabs(dy);
                if (ox > 0 && oy > 0) {
                    double nx = dx >= 0 ? 1 : -1, ny = dy >= 0 ? 1 : -1;
                    double p = 6.0 * dt;
                    a.vx -= nx*p; a.vy -= ny*p; b.vx += nx*p; b.vy += ny*p;
                }
            }
        for (auto& m : movers) {
            double sp = std::sqrt(m.vx*m.vx + m.vy*m.vy), cap = 16.0;
            if (sp > cap) { m.vx = m.vx/sp*cap; m.vy = m.vy/sp*cap; }
            place(m, false);
        }
    }

    void idle() override {
        TApplication::idle();
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration_cast<std::chrono::microseconds>(
                        now - last).count() / 1e6;
        if (dt < 1.0/45.0) return;
        last = now;
        if (!frozen) step(dt);
    }
};

int main() {
    TMeltApp app;
    app.run();
    return 0;
}
