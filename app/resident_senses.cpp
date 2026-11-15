// Resident senses. See resident_senses.h and docs/resident-senses.md.
#define Uses_TApplication
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TDialog
#define Uses_TFrame
#define Uses_TMenuView
#define Uses_TDrawBuffer
#define Uses_TScreen
#define Uses_TEvent
#define Uses_TView
#include <tvision/tv.h>

#include "resident_senses.h"
#include "resident_overlay.h"
#include "describe.h"

#include <cstdlib>
#include <cxxabi.h>
#include <typeinfo>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

extern TView *TheTopView;

namespace resident {
namespace {

using Clock = std::chrono::steady_clock;

std::string esc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o;
}
std::string q(const std::string& s) { return "\"" + esc(s) + "\""; }

std::string str(const KV& kv, const char* k, const std::string& def = "") {
    auto it = kv.find(k);
    return it == kv.end() ? def : it->second;
}
bool has(const KV& kv, const char* k) { return kv.find(k) != kv.end(); }
int num(const KV& kv, const char* k, int def) {
    auto it = kv.find(k);
    return it == kv.end() || it->second.empty() ? def : std::atoi(it->second.c_str());
}

// The reader's name: reader=, else actor=, else "resident".
std::string readerOf(const KV& kv) {
    std::string r = str(kv, "reader");
    if (r.empty()) r = str(kv, "actor");
    return r.empty() ? "resident" : r;
}

// ---------------------------------------------------------------- the desktop as it is

// Desktop windows, front first.
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

// Windows in the application group in front of the desktop (dialogs from messageBox), front first.
std::vector<TWindow*> appWindows() {
    std::vector<TWindow*> out;
    TGroup* a = TProgram::application;
    if (!a || !a->first()) return out;
    TView* f = a->first();
    TView* v = f;
    do {
        if (v == TProgram::deskTop) break;
        if (auto* w = dynamic_cast<TWindow*>(v)) if (w->state & sfVisible) out.push_back(w);
        v = v->next;
    } while (v != f);
    return out;
}

bool onDesk(TWindow* w) { return w && w->owner == TProgram::deskTop; }

// A window's rectangle in screen cells.
TRect screenRect(TWindow* w) {
    TRect r = w->getBounds();
    if (w->owner) {
        TPoint o{0, 0};
        for (TView* g = w->owner; g; g = g->owner) { o.x += g->origin.x; o.y += g->origin.y; }
        r.move(o.x, o.y);
    }
    return r;
}

// The views that sit in front of w (their screen rectangles).
std::vector<TRect> coverersOf(TWindow* w) {
    std::vector<TRect> out;
    for (TWindow* a : appWindows()) if (a != w) out.push_back(screenRect(a));
    if (!onDesk(w)) return out;
    for (TWindow* o : deskWindows()) {
        if (o == w) break;
        if (o->state & sfVisible) out.push_back(screenRect(o));
    }
    return out;
}

TRect deskScreenRect() {
    TDeskTop* d = TProgram::deskTop;
    if (!d) return TRect(0, 0, 0, 0);
    TRect r = d->getExtent();
    r.move(d->origin.x, d->origin.y);
    return r;
}

bool cellSeen(int x, int y, const TRect& clip, const std::vector<TRect>& cover) {
    if (!clip.contains(TPoint{x, y})) return false;
    for (const auto& c : cover) if (c.contains(TPoint{x, y})) return false;
    return true;
}

// How much of the window the person can see: cells not covered by views in front of it.
void coverage(TWindow* w, int& seen, int& total) {
    seen = total = 0;
    if (!(w->state & sfVisible)) { total = w->size.x * w->size.y; return; }
    TRect r = screenRect(w);
    TRect clip = onDesk(w) ? deskScreenRect() : TRect(0, 0, TScreen::screenWidth, TScreen::screenHeight);
    auto cover = coverersOf(w);
    for (int y = r.a.y; y < r.b.y; ++y)
        for (int x = r.a.x; x < r.b.x; ++x) {
            ++total;
            if (cellSeen(x, y, clip, cover)) ++seen;
        }
}

std::string cellText(const TScreenCell& c) {
    if (c.character.isWideCharTrail()) return "";
    TStringView t = c.character.getText();
    if (t.size() == 0 || (t.size() == 1 && t[0] == '\0')) return " ";
    return std::string(t.data(), t.size());
}

// The window's cached picture (TGroup::buffer): every cell, covered parts included.
// Fresh only while some part of the window is exposed: a fully covered window does not redraw.
const TScreenCell* bufferOf(TWindow* w) { return w ? w->buffer : nullptr; }

std::vector<std::string> bufferLines(TWindow* w, bool inner) {
    std::vector<std::string> out;
    const TScreenCell* b = bufferOf(w);
    if (!b) return out;
    int x0 = 0, y0 = 0, x1 = w->size.x, y1 = w->size.y;
    if (inner && w->frame) { x0 = 1; y0 = 1; x1 = std::max(1, x1 - 1); y1 = std::max(1, y1 - 1); }
    for (int y = y0; y < y1; ++y) {
        std::string line;
        for (int x = x0; x < x1; ++x) line += cellText(b[y * w->size.x + x]);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        out.push_back(line);
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

uint64_t fnv(const void* p, size_t n, uint64_t h = 1469598103934665603ull) {
    const unsigned char* c = (const unsigned char*)p;
    for (size_t i = 0; i < n; ++i) { h ^= c[i]; h *= 1099511628211ull; }
    return h;
}

uint64_t contentHash(TWindow* w) {
    std::vector<std::string> lines;
    if (hooks().textOf && hooks().textOf(w, lines)) {
        uint64_t h = 1469598103934665603ull;
        for (auto& l : lines) { h = fnv(l.data(), l.size(), h); h = fnv("\n", 1, h); }
        return h;
    }
    const TScreenCell* b = bufferOf(w);
    if (!b) return 0;
    return fnv(b, sizeof(TScreenCell) * (size_t)w->size.x * (size_t)w->size.y);
}

std::string idOfW(TWindow* w) {
    std::string id = hooks().idOf ? hooks().idOf(w) : "";
    return id;
}

std::string typeOfW(TWindow* w) {
    if (!onDesk(w) || dynamic_cast<TDialog*>(w)) {
        if (dynamic_cast<TDialog*>(w)) return "dialog";
    }
    if (hooks().typeOf) return hooks().typeOf(w);
    return "window";
}

std::string titleOf(TWindow* w) {
    const char* t = w->getTitle(0);
    return t ? t : "";
}

// The first n characters (code points) of s.
std::string headChars(const std::string& s, size_t n) {
    size_t i = 0, k = 0;
    while (i < s.size() && k < n) {
        unsigned char c = (unsigned char)s[i];
        i += c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : (c >> 3) == 0x1e ? 4 : 1;
        ++k;
    }
    return i < s.size() ? s.substr(0, std::min(i, s.size())) + "..." : s;
}

std::string firstLineOf(const std::vector<std::string>& lines) {
    for (const auto& l : lines) {
        size_t i = l.find_first_not_of(' ');
        if (i == std::string::npos) continue;
        return headChars(l.substr(i), 45);
    }
    return "";
}

// One sentence of state: from the window's own model when its type has one, else from its cells.
std::string sentenceOf(TWindow* w) {
    if (hooks().stateOf) {
        std::string s = hooks().stateOf(w);
        if (!s.empty()) return s;
    }
    if (dynamic_cast<TDialog*>(w)) { std::vector<std::string> t; return ww::dialogState(w, t); }
    auto lines = bufferLines(w, true);
    int marks = 0;
    for (auto& l : lines) for (char c : l) if (c != ' ') ++marks;
    if (!marks) return "blank";
    return std::to_string(marks) + " characters drawn; top line: \"" + firstLineOf(lines) + "\"";
}

std::string seenWord(int seen, int total) {
    if (seen <= 0) return "none";
    if (seen >= total) return "all";
    return "part";
}


// The C++ class of a window's describer (or of the window when it has none).
std::string classOf(const void* p, const std::type_info& ti) {
    (void)p;
    int st = 0;
    char* d = abi::__cxa_demangle(ti.name(), nullptr, nullptr, &st);
    std::string s = (st == 0 && d) ? d : ti.name();
    std::free(d);
    return s;
}

// One window by the describe contract: state and text from its model; cells only when it has no text model.
std::string describeOne(TWindow* w, bool withText, int limit) {
    TDeskTop* d = TProgram::deskTop;
    std::vector<std::string> text;
    std::string state, source = "model", cls;
    bool contract = true;
    bool modelText = false;
    if (ww::describeWindow(w, state, text, modelText, cls)) {
        if (!modelText) source = "cells";
    } else if (dynamic_cast<TDialog*>(w)) {
        state = ww::dialogState(w, text);
        cls = "dialog";
        source = "controls";
    } else {
        contract = false;
        cls = classOf(w, typeid(*w));
        state = sentenceOf(w);
        source = "cells";
    }
    if (source == "cells" && withText && bufferOf(w)) text = bufferLines(w, true);
    int seen, total;
    coverage(w, seen, total);
    TRect r = w->getBounds();
    std::ostringstream os;
    os << "{\"id\":" << q(idOfW(w)) << ",\"type\":" << q(typeOfW(w)) << ",\"class\":" << q(cls)
       << ",\"title\":" << q(titleOf(w)) << ",\"owner\":" << q(ownerOf(w)) << ",\"x\":" << r.a.x << ",\"y\":" << r.a.y
       << ",\"w\":" << w->size.x << ",\"h\":" << w->size.y
       << ",\"focused\":" << ((d && d->current == w) ? "true" : "false") << ",\"seen\":" << q(seenWord(seen, total))
       << ",\"contract\":" << (contract ? "true" : "false") << ",\"state\":" << q(state)
       << ",\"standing\":\"data\",\"text_from\":" << q(source);
    if (withText) {
        os << ",\"count\":" << text.size() << ",\"text\":[";
        int n = 0;
        for (size_t i = 0; i < text.size() && n < limit; ++i, ++n) os << (n ? "," : "") << q(text[i]);
        os << "]";
    }
    os << "}";
    return os.str();
}

// ---------------------------------------------------------------- marks

struct Mark {
    std::string winId;
    TOverlayCell* view = nullptr;
    Clock::time_point until{};
};

std::map<std::string, Mark> g_marks;   // reader -> its mark
bool g_marksOn = true;
int g_markMs = 3000;

TWindow* windowById(const std::string& id) {
    return hooks().findWindow ? hooks().findWindow(id) : nullptr;
}

// The first frame cell of w (top row, then left column) that the person can see.
bool markCell(TWindow* w, TPoint& at) {
    if (!w || !(w->state & sfVisible) || w->size.x < 3) return false;
    TRect r = screenRect(w);
    TRect clip = onDesk(w) ? deskScreenRect() : TRect(0, 0, TScreen::screenWidth, TScreen::screenHeight);
    auto cover = coverersOf(w);
    for (int x = r.a.x + 1; x < r.b.x - 1; ++x)
        if (cellSeen(x, r.a.y, clip, cover)) { at = {x, r.a.y}; return true; }
    for (int y = r.a.y + 1; y < r.b.y - 1; ++y)
        if (cellSeen(r.a.x, y, clip, cover)) { at = {r.a.x, y}; return true; }
    return false;
}

void dropMark(Mark& m) {
    if (m.view) { TObject::destroy(m.view); m.view = nullptr; }
}

void placeMark(Mark& m, const std::string& reader) {
    TWindow* w = windowById(m.winId);
    TPoint at;
    if (!g_marksOn || !w || !markCell(w, at)) {
        if (m.view && (m.view->state & sfVisible)) m.view->hide();
        return;
    }
    TColorAttr a = w->frame ? w->frame->getColor(1)[0] : TColorAttr(TColorBIOS(7), TColorBIOS(1));
    a = a.reversed();
    std::string t(1, reader.empty() ? 'r' : (char)std::tolower((unsigned char)reader[0]));
    if (!m.view) {
        m.view = new TOverlayCell(TRect(at.x, at.y, at.x + 1, at.y + 1));
        m.view->text = t;
        m.view->attr = a;
        TProgram::application->insert(m.view);
        return;
    }
    bool redraw = m.view->text != t || !(m.view->attr == a);
    m.view->text = t;
    m.view->attr = a;
    if (m.view->origin.x != at.x || m.view->origin.y != at.y) m.view->moveTo(at.x, at.y);
    if (!(m.view->state & sfVisible)) m.view->show();
    else if (redraw) m.view->drawView();
}

void markReading(const std::string& reader, TWindow* w) {
    if (!g_marksOn || !w) return;
    std::string id = idOfW(w);
    if (id.empty()) return;
    Mark& m = g_marks[reader];
    if (m.winId != id && m.view) m.view->hide();
    m.winId = id;
    m.until = Clock::now() + std::chrono::milliseconds(g_markMs);
    placeMark(m, reader);
}

// ---------------------------------------------------------------- per-reader snapshots

struct Snap {
    std::string type, title;
    TRect r;
    int z = 0;
    uint64_t hash = 0;
    bool seen = false;
};

struct ReaderView {
    std::map<std::string, Snap> wins;   // by window id
    std::string focus;
    Clock::time_point at{};
};

std::map<std::string, ReaderView> g_readers;
const size_t kReadersMax = 64;

ReaderView snapshotNow() {
    ReaderView v;
    int z = 0;
    for (TWindow* w : deskWindows()) {
        std::string id = idOfW(w);
        if (id.empty()) { ++z; continue; }
        Snap s;
        s.type = typeOfW(w);
        s.title = titleOf(w);
        s.r = w->getBounds();
        s.z = z++;
        s.hash = contentHash(w);
        int seen, total;
        coverage(w, seen, total);
        s.seen = seen > 0;
        v.wins[id] = s;
    }
    TDeskTop* d = TProgram::deskTop;
    if (d && d->current) if (auto* w = dynamic_cast<TWindow*>(d->current)) v.focus = idOfW(w);
    v.at = Clock::now();
    return v;
}

std::string rectJson(const TRect& r) {
    std::ostringstream os;
    os << "[" << r.a.x << "," << r.a.y << "," << (r.b.x - r.a.x) << "," << (r.b.y - r.a.y) << "]";
    return os.str();
}

// What changed between the reader's last look and now; stores now as its last look.
std::string changesJson(const std::string& reader) {
    ReaderView now = snapshotNow();
    auto it = g_readers.find(reader);
    std::ostringstream os;
    os << "{\"reader\":" << q(reader);
    if (it == g_readers.end()) {
        if (g_readers.size() >= kReadersMax) g_readers.erase(g_readers.begin());
        os << ",\"first\":true,\"windows\":" << now.wins.size() << "}";
        g_readers[reader] = now;
        return os.str();
    }
    ReaderView& was = it->second;
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.at - was.at).count();
    os << ",\"first\":false,\"since_ms\":" << ms;
    std::ostringstream opened, closed, moved, retitled, restacked, content;
    bool fo = true, fc = true, fm = true, fr = true, fz = true, fx = true;
    int unchanged = 0;
    for (auto& kv : now.wins) {
        auto p = was.wins.find(kv.first);
        const Snap& s = kv.second;
        if (p == was.wins.end()) {
            opened << (fo ? "" : ",") << "{\"id\":" << q(kv.first) << ",\"type\":" << q(s.type) << ",\"title\":" << q(s.title) << "}";
            fo = false;
            continue;
        }
        const Snap& o = p->second;
        bool any = false;
        if (o.r != s.r) {
            moved << (fm ? "" : ",") << "{\"id\":" << q(kv.first) << ",\"from\":" << rectJson(o.r) << ",\"to\":" << rectJson(s.r) << "}";
            fm = false; any = true;
        }
        if (o.title != s.title) {
            retitled << (fr ? "" : ",") << "{\"id\":" << q(kv.first) << ",\"from\":" << q(o.title) << ",\"to\":" << q(s.title) << "}";
            fr = false; any = true;
        }
        if (o.z != s.z) {
            restacked << (fz ? "" : ",") << "{\"id\":" << q(kv.first) << ",\"from\":" << o.z << ",\"to\":" << s.z << "}";
            fz = false; any = true;
        }
        if (o.hash != s.hash) {
            content << (fx ? "" : ",") << q(kv.first);
            fx = false; any = true;
        }
        if (!any) ++unchanged;
    }
    for (auto& kv : was.wins) {
        if (now.wins.count(kv.first)) continue;
        closed << (fc ? "" : ",") << "{\"id\":" << q(kv.first) << ",\"type\":" << q(kv.second.type) << ",\"title\":" << q(kv.second.title) << "}";
        fc = false;
    }
    os << ",\"opened\":[" << opened.str() << "],\"closed\":[" << closed.str() << "],\"moved\":[" << moved.str()
       << "],\"retitled\":[" << retitled.str() << "],\"restacked\":[" << restacked.str() << "],\"content\":["
       << content.str() << "]";
    if (now.focus != was.focus) os << ",\"focus\":{\"from\":" << q(was.focus) << ",\"to\":" << q(now.focus) << "}";
    os << ",\"unchanged\":" << unchanged << "}";
    was = now;
    return os.str();
}

}  // namespace

// ---------------------------------------------------------------- commands

std::string cmdGlance(const KV& kv) {
    std::ostringstream os;
    TDeskTop* d = TProgram::deskTop;
    os << "{\"windows\":[";
    bool first = true;
    int z = 0;
    auto one = [&](TWindow* w, int zz) {
        int seen, total;
        coverage(w, seen, total);
        TRect r = w->getBounds();
        os << (first ? "" : ",") << "{\"id\":" << q(idOfW(w)) << ",\"type\":" << q(typeOfW(w))
           << ",\"title\":" << q(titleOf(w)) << ",\"x\":" << r.a.x << ",\"y\":" << r.a.y
           << ",\"w\":" << w->size.x << ",\"h\":" << w->size.y << ",\"z\":" << zz
           << ",\"owner\":" << q(ownerOf(w))
           << ",\"focused\":" << ((d && d->current == w) ? "true" : "false")
           << ",\"seen\":" << q(seenWord(seen, total))
           << ",\"seen_pct\":" << (total ? (seen * 100 + total / 2) / total : 0)
           << ",\"state\":" << q(sentenceOf(w)) << "}";
        first = false;
    };
    for (TWindow* w : appWindows()) one(w, -1);
    for (TWindow* w : deskWindows()) one(w, z++);
    os << "],\"focus\":" << q(d && d->current ? idOfW(dynamic_cast<TWindow*>(d->current)) : "")
       << ",\"modal\":" << (TheTopView ? "true" : "false")
       << ",\"menu_open\":" << (dynamic_cast<TMenuView*>(TheTopView) ? "true" : "false");
    if (has(kv, "reader") || has(kv, "since")) os << ",\"changes\":" << changesJson(readerOf(kv));
    os << "}";
    return os.str();
}

std::string cmdRead(const KV& kv) {
    std::string id = str(kv, "id");
    if (id.empty()) return "err missing id";
    TWindow* w = windowById(id);
    if (!w) return "err window not found: " + id;
    std::vector<std::string> lines;
    std::string from = "model";
    bool fresh = true;
    if (!(hooks().textOf && hooks().textOf(w, lines))) {
        from = "cells";
        if (!bufferOf(w)) return "err window has no picture yet: " + id;
        lines = bufferLines(w, str(kv, "frame") != "1");
        int seen, total;
        coverage(w, seen, total);
        fresh = seen > 0;
    }
    int limit = std::max(1, std::min(num(kv, "limit", 2000), 20000));
    int from_line = std::max(0, num(kv, "from", 0));
    std::ostringstream os;
    os << "{\"id\":" << q(id) << ",\"type\":" << q(typeOfW(w)) << ",\"title\":" << q(titleOf(w))
       << ",\"standing\":\"data\",\"from\":" << q(from) << ",\"fresh\":" << (fresh ? "true" : "false")
       << ",\"count\":" << lines.size() << ",\"lines\":[";
    int n = 0;
    for (size_t i = (size_t)from_line; i < lines.size() && n < limit; ++i, ++n)
        os << (n ? "," : "") << q(lines[i]);
    os << "]}";
    markReading(readerOf(kv), w);
    return os.str();
}

std::string cmdStudy(const KV& kv) {
    std::string id = str(kv, "id");
    TWindow* w = nullptr;
    const TScreenCell* cells = nullptr;
    int cw = 0, ch = 0, ox = 0, oy = 0;
    std::string from;
    if (!id.empty()) {
        w = windowById(id);
        if (!w) return "err window not found: " + id;
        cells = bufferOf(w);
        if (!cells) return "err window has no picture yet: " + id;
        cw = w->size.x; ch = w->size.y;
        from = "window";
    } else {
        cells = TScreen::screenBuffer;
        if (!cells) return "err no screen";
        cw = TScreen::screenWidth; ch = TScreen::screenHeight;
        from = "screen";
        if (str(kv, "screen") != "1" && TProgram::deskTop) oy = TProgram::deskTop->origin.y, ox = TProgram::deskTop->origin.x;
    }
    int x = num(kv, "x", 0), y = num(kv, "y", 0);
    int wdt = num(kv, "w", cw - x - ox), hgt = num(kv, "h", ch - y - oy);
    int sx = x + ox, sy = y + oy;
    if (sx < 0 || sy < 0 || wdt <= 0 || hgt <= 0 || sx + wdt > cw || sy + hgt > ch)
        return "err rectangle outside " + from + " (" + std::to_string(cw) + "x" + std::to_string(ch - oy) + ")";
    if ((long)wdt * hgt > 20000) return "err rectangle larger than 20000 cells";
    std::string tens, units;
    for (int i = 0; i < wdt; ++i) {
        int col = x + i;
        tens += (col % 10 == 0) ? (char)('0' + (col / 10) % 10) : ' ';
        units += (char)('0' + col % 10);
    }
    std::ostringstream os;
    os << "{\"standing\":\"data\",\"from\":" << q(from);
    if (w) os << ",\"id\":" << q(id);
    os << ",\"x\":" << x << ",\"y\":" << y << ",\"w\":" << wdt << ",\"h\":" << hgt
       << ",\"ruler\":[" << q(tens) << "," << q(units) << "],\"runs_are\":\"[x, n, fg, bg] in BIOS colours 0-15\",\"rows\":[";
    for (int r = 0; r < hgt; ++r) {
        std::string text;
        std::ostringstream runs;
        int runX = 0, runN = 0, runA = -1;
        bool fr = true;
        for (int i = 0; i < wdt; ++i) {
            const TScreenCell& c = cells[(sy + r) * cw + sx + i];
            text += cellText(c);
            int a = c.attribute.toBIOS();
            if (a != runA) {
                if (runN) { runs << (fr ? "" : ",") << "[" << runX << "," << runN << "," << (runA & 15) << "," << ((runA >> 4) & 15) << "]"; fr = false; }
                runX = i; runN = 0; runA = a;
            }
            ++runN;
        }
        if (runN) runs << (fr ? "" : ",") << "[" << runX << "," << runN << "," << (runA & 15) << "," << ((runA >> 4) & 15) << "]";
        os << (r ? "," : "") << "{\"y\":" << (y + r) << ",\"text\":" << q(text) << ",\"runs\":[" << runs.str() << "]}";
    }
    os << "]}";
    if (w) markReading(readerOf(kv), w);
    return os.str();
}

std::string cmdDescribe(const KV& kv) {
    std::string id = str(kv, "id");
    int limit = std::max(0, std::min(num(kv, "limit", 2000), 20000));
    if (!id.empty()) {
        TWindow* w = windowById(id);
        if (!w) return "err window not found: " + id;
        return describeOne(w, str(kv, "text") != "0", limit);
    }
    bool withText = str(kv, "text") == "1";
    std::ostringstream os;
    os << "{\"windows\":[";
    bool first = true;
    for (TWindow* w : appWindows()) { os << (first ? "" : ",") << describeOne(w, withText, limit); first = false; }
    for (TWindow* w : deskWindows()) { os << (first ? "" : ",") << describeOne(w, withText, limit); first = false; }
    os << "]}";
    return os.str();
}

std::string cmdChanges(const KV& kv) { return changesJson(readerOf(kv)); }

bool marksOn() { return g_marksOn; }

void setMarksOn(bool on) {
    g_marksOn = on;
    if (!g_marksOn) for (auto& m : g_marks) dropMark(m.second);
}

std::string cmdMark(const KV& kv) {
    bool wantOn = g_marksOn;
    int wantMs = g_markMs;
    if (has(kv, "on")) {
        std::string v = str(kv, "on");
        wantOn = (v == "1" || v == "true" || v == "on");
    }
    if (has(kv, "ms")) wantMs = std::max(200, std::min(num(kv, "ms", g_markMs), 600000));
    if (wantOn != g_marksOn || wantMs != g_markMs) {
        std::string refusal = ruleGate(std::string("reading marks ") + (wantOn ? "on" : "off") + ", " + std::to_string(wantMs) + " ms");
        if (!refusal.empty()) return refusal;
        setMarksOn(wantOn);
        g_markMs = wantMs;
    }
    std::ostringstream os;
    os << "{\"on\":" << (g_marksOn ? "true" : "false") << ",\"ms\":" << g_markMs << ",\"marks\":[";
    bool first = true;
    for (auto& m : g_marks) {
        if (!m.second.view || !(m.second.view->state & sfVisible)) continue;
        os << (first ? "" : ",") << "{\"reader\":" << q(m.first) << ",\"window\":" << q(m.second.winId)
           << ",\"x\":" << m.second.view->origin.x << ",\"y\":" << m.second.view->origin.y << "}";
        first = false;
    }
    os << "]}";
    return os.str();
}

void tickSenses() {
    if (g_marks.empty()) return;
    auto now = Clock::now();
    for (auto it = g_marks.begin(); it != g_marks.end();) {
        Mark& m = it->second;
        if (!g_marksOn || now >= m.until || !windowById(m.winId)) {
            dropMark(m);
            it = g_marks.erase(it);
            continue;
        }
        placeMark(m, it->first);
        ++it;
    }
}

}  // namespace resident
