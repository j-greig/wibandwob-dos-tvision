// Mirror: see mirror_view.h.

#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TWindow
#define Uses_TDeskTop
#define Uses_TScreen
#define Uses_TEvent
#include <tvision/tv.h>

#include "mirror_view.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

TMirrorView::TMirrorView(const TRect& r, TDeskTop* desk, TView* source, bool flip, bool block) :
    TView(r), desk(desk), source(source), flip(flip), block(block)
{
    growMode = gfGrowHiX | gfGrowHiY;
    eventMask |= evBroadcast;
}

void TMirrorView::shutDown()
{
    if (timer) killTimer(timer);
    timer = 0;
    TView::shutDown();
}

void TMirrorView::handleEvent(TEvent& ev)
{
    TView::handleEvent(ev);
    if (ev.what == evBroadcast && ev.message.command == cmTimerExpired && timer && ev.message.infoPtr == timer)
        drawView();   // the timer event goes on to other views; it is not cleared
}

static bool blankCell(const TScreenCell& c)
{
    TStringView t = c._ch.getText();
    return c._ch.isWideCharTrail() || t.size() == 0 || (t.size() == 1 && (t[0] == ' ' || t[0] == 0));
}

void TMirrorView::blank()
{
    TDrawBuffer b;
    b.moveChar(0, ' ', getColor(1), size.x);
    writeLine(0, 0, size.x, size.y, b);
}

void TMirrorView::draw()
{
    // No timer of its own: the stage redraws its mirrors at every tick and after every seek (stageMirrorSettle),
    // so what a mirror shows depends on the tick, not on how many 125 ms timer events it has seen.
    int W = size.x, H = size.y;
    std::vector<TScreenCell> row(W > 0 ? W : 1);
    TScreenCell blank;
    blank._ch.moveChar(' ');
    ::setAttr(blank, getColor(1));
    const TScreenCell* scr = TScreen::screenBuffer;
    int sw = TScreen::screenWidth, sh = TScreen::screenHeight;
    if (source && desk->indexOf(source) == 0) source = nullptr;   // closed
    TView* src = source ? source : desk;
    TPoint a = src->makeGlobal(TPoint {0, 0});
    int SW = src->size.x, SH = src->size.y;
    if (!scr || W <= 0 || H <= 0 || SW <= 0 || SH <= 0) return;
    // Copy the source rectangle first: the mirror may lie inside it, and it writes as it goes.
    std::vector<TScreenCell> snap(SW * SH, blank);
    for (int y = 0; y < SH; ++y)
        for (int x = 0; x < SW; ++x) {
            int gx = a.x + x, gy = a.y + y;
            if (gx >= 0 && gy >= 0 && gx < sw && gy < sh) snap[y * SW + x] = scr[gy * sw + gx];
        }
    auto at = [&](int x, int y) -> const TScreenCell& { return snap[y * SW + x]; };
    auto key = [](const TScreenCell& c) { uint32_t k = 0; memcpy(&k, &c._ch._text[0], 4); return k; };
    // block: the source's commonest character is its background fill; a block shows something else when it can
    uint32_t fill = 0;
    if (block) {
        std::map<uint32_t, int> n; int top = 0;
        for (auto& c : snap) if (++n[key(c)] > top) { top = n[key(c)]; fill = key(c); }
    }
    for (int y = 0; y < H; ++y) {
        int sy0 = y * SH / H, sy1 = std::max(sy0 + 1, (y + 1) * SH / H);
        for (int x = 0; x < W; ++x) {
            int ox = flip ? W - 1 - x : x;
            int sx0 = ox * SW / W, sx1 = std::max(sx0 + 1, (ox + 1) * SW / W);
            int sx = sx0, sy = sy0;
            if (block) {
                // the most frequent character of the block that is neither blank nor the fill;
                // ties go to the first seen; none found keeps the nearest cell
                int best = 0;
                for (int j = sy0; j < sy1 && j < SH; ++j)
                    for (int i = sx0; i < sx1 && i < SW; ++i) {
                        if (blankCell(at(i, j)) || key(at(i, j)) == fill) continue;
                        int n = 0;
                        for (int jj = sy0; jj < sy1 && jj < SH; ++jj)
                            for (int ii = sx0; ii < sx1 && ii < SW; ++ii)
                                if (key(at(ii, jj)) == key(at(i, j))) ++n;
                        if (n > best) { best = n; sx = i; sy = j; }
                    }
            }
            if (at(sx, sy)._ch.isWideCharTrail() && sx > 0) --sx;   // take the head of a wide character
            const TScreenCell& c = at(sx, sy);
            if (c._ch.isWide()) {
                // a wide character needs two output cells; with no room it becomes a space
                if (x + 1 < W && sx + 1 < SW) { row[x] = c; row[x + 1] = at(sx + 1, sy); ++x; }
                else { row[x] = blank; ::setAttr(row[x], ::getAttr(c)); }
            } else if (c._ch.isWideCharTrail()) {
                row[x] = blank; ::setAttr(row[x], ::getAttr(c));
            } else
                row[x] = c;
        }
        writeBuf(0, y, W, 1, row.data());
    }
}

// A mirror copies the screen, and may show itself or another mirror, so one redraw is not the picture: each pass
// carries the copy one level deeper. full: a pass for each mirror (a chain of mirrors of mirrors) and six more (a
// mirror that shows itself reaches cells smaller than a character by then). Not full: one pass, the rate the old
// 125 ms timer gave, so a playing film costs what it did.
void stageMirrorSettle(TDeskTop* desk, bool full)
{
    std::vector<TMirrorView*> ms;
    desk->forEach([](TView* v, void* out) {
        if (auto* g = dynamic_cast<TGroup*>(v))
            g->forEach([](TView* c, void* o) {
                if (auto* mv = dynamic_cast<TMirrorView*>(c)) ((std::vector<TMirrorView*>*) o)->push_back(mv);
            }, out);
    }, &ms);
    if (ms.empty()) return;
    const int passes = full ? std::min(24, (int) ms.size() + 6) : 1;
    // A mirror that shows itself has more than one picture it can settle on (block mode keeps the commonest
    // character, which depends on what was there). A full settle starts every mirror from blank, so the result
    // depends on the desktop alone, not on what the mirrors showed before.
    if (full)
        for (auto* m : ms) m->blank();
    for (int pass = 0; pass < passes; ++pass)
        for (auto* m : ms) m->drawView();
}

void stageMirror(TWindow* w, TDeskTop* desk, TView* source, bool flip, bool block)
{
    TMirrorView* m = nullptr;
    w->forEach([](TView* v, void* out) {
        if (auto* mv = dynamic_cast<TMirrorView*>(v)) *(TMirrorView**) out = mv;
    }, &m);
    if (!m) {
        TRect r = w->getExtent();
        if (w->frame) r.grow(-1, -1);
        w->insert(m = new TMirrorView(r, desk, source, flip, block));
    }
    m->desk = desk; m->source = source; m->flip = flip; m->block = block;
    m->drawView();
}
