#include "midi_view.h"

#define Uses_TDrawBuffer
#define Uses_TProgram
#include <tvision/tv.h>

#include "midi.h"

#include <algorithm>
#include <cstdio>

using namespace sound;

TMidiView::TMidiView(const TRect& bounds) : TView(bounds) {
    growMode = gfGrowHiX | gfGrowHiY;
    eventMask |= evBroadcast;
    timer_ = setTimer(100, 100);
}

TMidiView::~TMidiView() {
    if (timer_) killTimer(timer_);
}

std::string TMidiView::line(double ms, bool out, const std::string& from, const std::string& text) {
    char b[160];
    std::snprintf(b, sizeof(b), "%s %9.3f  %-9s %s", out ? "OUT" : "IN ", ms / 1000.0, from.substr(0, 9).c_str(), text.c_str());
    return b;
}

void TMidiView::handleEvent(TEvent& ev) {
    TView::handleEvent(ev);
    if (ev.what == evBroadcast && ev.message.command == cmTimerExpired && timer_ && ev.message.infoPtr == timer_) {
        drawView();
        clearEvent(ev);
    }
}

// The last messages as lines, a run of clock ticks folded into one.
static void foldedLines(std::vector<std::string>& lines, std::vector<bool>& outs) {
    int ticks = 0;
    double tickMs = 0;
    for (const auto& m : midi::recent(400)) {
        if (m.bytes.size() == 1 && m.bytes[0] == 0xF8) { ++ticks; tickMs = m.ms; continue; }
        if (ticks) { lines.push_back(TMidiView::line(tickMs, true, "clock", std::to_string(ticks) + " clock ticks")); outs.push_back(true); ticks = 0; }
        lines.push_back(TMidiView::line(m.ms, m.out, m.from, midi::describe(m.bytes)));
        outs.push_back(m.out);
    }
    if (ticks) { lines.push_back(TMidiView::line(tickMs, true, "clock", std::to_string(ticks) + " clock ticks")); outs.push_back(true); }
}

void TMidiView::draw() {
    TColorAttr base, dim, bright, outA;
    TColorAttr themed;
    if (ww::contentAttr(this, 15, themed)) {
        const TColor bg = themed.getBackground();
        base = themed;
        dim = TColorAttr(ww::egaRGB(7), bg);
        bright = TColorAttr(ww::egaRGB(15), bg);
        outA = TColorAttr(ww::egaRGB(11), bg);
    } else {
        const TColorRGB bk(0, 0, 0);
        base = TColorAttr(TColorRGB(0xAA, 0xAA, 0xAA), bk);
        dim = TColorAttr(TColorRGB(0x66, 0x66, 0x66), bk);
        bright = TColorAttr(TColorRGB(0xFF, 0xFF, 0xFF), bk);
        outA = bright;
    }
    const int W = size.x, H = size.y;
    std::vector<std::string> lines;
    std::vector<bool> outs;
    foldedLines(lines, outs);
    const int rows = std::max(0, H - 2);
    const int first = std::max(0, (int)lines.size() - rows);
    for (int y = 0; y < H; ++y) {
        TDrawBuffer b;
        b.moveChar(0, ' ', base, (ushort)W);
        if (y == 0) {
            char hdr[160];
            std::snprintf(hdr, sizeof(hdr), "MIDI  device %s  %d sources  in %ld  out %ld  clock out %s", midi::device(),
                          midi::sourcesConnected(), midi::inCount(), midi::outCount(), midi::clockOut() ? "on" : "off");
            b.moveStr(1, hdr, bright);
        } else if (y >= 2) {
            const int i = first + (y - 2);
            if (i < (int)lines.size()) b.moveStr(1, lines[i], outs[i] ? outA : base, (ushort)std::max(0, W - 2));
        }
        writeLine(0, (short)y, (short)W, 1, b);
    }
}

std::string TMidiView::describeState() const {
    auto r = midi::recent(1);
    std::string last = r.empty() ? "nothing yet" : std::string(r[0].out ? "out " : "in ") + midi::describe(r[0].bytes);
    return std::string("MIDI monitor, device ") + midi::device() + ", " + std::to_string(midi::inCount()) + " in, " +
           std::to_string(midi::outCount()) + " out; last: " + last;
}

bool TMidiView::describeText(std::vector<std::string>& out) const {
    std::vector<bool> outs;
    foldedLines(out, outs);
    return true;
}

TMidiWindow::TMidiWindow(const TRect& bounds)
    : TWindowInit(&TMidiWindow::initFrame), TWindow(bounds, "MIDI", wnNoNumber) {
    options |= ofTileable;
    TRect c = getExtent();
    c.grow(-1, -1);
    view_ = new TMidiView(c);
    insert(view_);
}
