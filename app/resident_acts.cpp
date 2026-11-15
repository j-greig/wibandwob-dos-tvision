// Resident authorship: who did what, window owners, one log of acts, the visible hand.
// See resident_senses.h and docs/resident-senses.md.
#define Uses_TApplication
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TDialog
#define Uses_TMenuView
#define Uses_TMenu
#define Uses_TMenuItem
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TDrawBuffer
#define Uses_TScreen
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TView
#include <tvision/tv.h>

#include "resident_senses.h"
#include "resident_overlay.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <vector>

extern TView *TheTopView;

namespace resident {
namespace {

using Clock = std::chrono::steady_clock;
const Clock::time_point g_t0 = Clock::now();

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_t0).count();
}

std::string esc(const std::string& s) {
    std::string o;
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

// ---------------------------------------------------------------- who

std::vector<Who> g_stack;
Who g_eventWho{"app", Source::App};
Who g_putWho{"app", Source::App};

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

std::string idOfV(TView* v) {
    auto* w = dynamic_cast<TWindow*>(v);
    if (!w || !hooks().idOf) return "";
    return hooks().idOf(w);
}

std::string typeOfV(TWindow* w) {
    if (dynamic_cast<TDialog*>(w)) return "dialog";
    return hooks().typeOf ? hooks().typeOf(w) : "window";
}

TRect screenRectOf(TView* v) {
    TRect r = v->getBounds();
    TPoint o{0, 0};
    for (TView* g = v->owner; g; g = g->owner) { o.x += g->origin.x; o.y += g->origin.y; }
    r.move(o.x, o.y);
    return r;
}

// The window under a screen point, front first.
TWindow* windowAtScreen(TPoint p) {
    for (TWindow* w : deskWindows())
        if ((w->state & sfVisible) && screenRectOf(w).contains(p)) return w;
    return nullptr;
}

std::string where(TPoint p) {
    TDeskTop* d = TProgram::deskTop;
    TPoint o = d ? d->origin : TPoint{0, 0};
    return std::to_string(p.x - o.x) + "," + std::to_string(p.y - o.y);
}

// What a person's pointer is over: a window id, the menu bar, the status line, the desktop.
std::string placeOf(TPoint p) {
    if (TheTopView && screenRectOf(TheTopView).contains(p)) {
        std::string id = idOfV(TheTopView);
        return !id.empty() ? id : dynamic_cast<TMenuView*>(TheTopView) ? "menu" : "dialog";
    }
    if (TProgram::menuBar && screenRectOf(TProgram::menuBar).contains(p)) return "menu bar";
    if (TProgram::statusLine && screenRectOf(TProgram::statusLine).contains(p)) return "status line";
    if (TWindow* w = windowAtScreen(p)) return idOfV(w);
    return "desktop";
}

std::string personKeyPlace() {
    if (TheTopView) {
        std::string id = idOfV(TheTopView);
        return !id.empty() ? id : dynamic_cast<TMenuView*>(TheTopView) ? "menu" : "dialog";
    }
    TDeskTop* d = TProgram::deskTop;
    return d && d->current ? idOfV(d->current) : "";
}

std::string keyName(const TEvent& ev) {
    ushort k = ev.keyDown.keyCode;
    ushort m = ev.keyDown.controlKeyState;
    struct N { ushort code; const char* name; };
    static const N names[] = {
        {kbEnter, "Enter"}, {kbEsc, "Esc"}, {kbTab, "Tab"}, {kbShiftTab, "Shift+Tab"}, {kbBack, "Backspace"},
        {kbUp, "Up"}, {kbDown, "Down"}, {kbLeft, "Left"}, {kbRight, "Right"}, {kbHome, "Home"}, {kbEnd, "End"},
        {kbPgUp, "PgUp"}, {kbPgDn, "PgDn"}, {kbIns, "Ins"}, {kbDel, "Del"},
        {kbCtrlLeft, "Ctrl+Left"}, {kbCtrlRight, "Ctrl+Right"}, {kbCtrlEnter, "Ctrl+Enter"},
        {kbF1, "F1"}, {kbF2, "F2"}, {kbF3, "F3"}, {kbF4, "F4"}, {kbF5, "F5"}, {kbF6, "F6"},
        {kbF7, "F7"}, {kbF8, "F8"}, {kbF9, "F9"}, {kbF10, "F10"}, {kbF11, "F11"}, {kbF12, "F12"},
        {kbAltF3, "Alt+F3"}, {kbShiftF6, "Shift+F6"},
    };
    for (const auto& n : names) if (n.code == k) return n.name;
    if (k >= 1 && k <= 26) return std::string("Ctrl+") + (char)('A' + k - 1);
    if ((m & kbAltShift) && ev.keyDown.textLength == 0) {
        static const char* row = "QWERTYUIOP";
        static const char* row2 = "ASDFGHJKL";
        static const char* row3 = "ZXCVBNM";
        uchar sc = (uchar)(k >> 8);
        if (sc >= 0x10 && sc <= 0x19) return std::string("Alt+") + row[sc - 0x10];
        if (sc >= 0x1e && sc <= 0x26) return std::string("Alt+") + row2[sc - 0x1e];
        if (sc >= 0x2c && sc <= 0x32) return std::string("Alt+") + row3[sc - 0x2c];
    }
    char b[16];
    std::snprintf(b, sizeof b, "0x%04x", k);
    return b;
}

// The menu item's name for a command ("" when no menu item carries it).
std::string menuName(TMenu* menu, ushort cmd, int depth = 0) {
    for (TMenuItem* it = menu ? menu->items : nullptr; it; it = it->next) {
        if (!it->name) continue;
        if (it->command == cmd && cmd != 0) {
            std::string s;
            for (const char* p = it->name; *p; ++p) if (*p != '~') s += *p;
            return s;
        }
        if (it->command == 0 && it->subMenu && depth < 6) {
            std::string r = menuName(it->subMenu, cmd, depth + 1);
            if (!r.empty()) return r;
        }
    }
    return "";
}

struct PeekMenuView2 : TMenuView {
    static TMenu* TMenuView::*menu_() { return &PeekMenuView2::menu; }
};

std::string commandName(ushort cmd) {
    TMenuView* bar = TProgram::menuBar;
    TMenu* m = bar ? bar->*PeekMenuView2::menu_() : nullptr;
    return menuName(m, cmd);
}

// ---------------------------------------------------------------- acts

struct Act {
    uint64_t n;
    long long ms;
    std::string actor;
    Source source;
    std::string what, window, result;
    uint64_t rseq;
};

std::deque<Act> g_acts;
uint64_t g_actN = 1;
uint64_t g_actDropped = 0;
const size_t kActsMax = 4000;

// Fold this act into the last one when it continues it (typing, a drag, a scroll).
bool mergeLast(const Who& w, const std::string& prefix, const std::string& window, long long withinMs, Act*& out) {
    if (g_acts.empty()) return false;
    Act& a = g_acts.back();
    if (a.actor != w.actor || a.source != w.source || a.window != window) return false;
    if (a.what.compare(0, prefix.size(), prefix) != 0) return false;
    if (nowMs() - a.ms > withinMs) return false;
    out = &a;
    return true;
}

// The person's mouse down, for turning a click into a drag on release.
uint64_t g_downAct = 0;
TPoint g_downAt{0, 0};

void logPerson(const TEvent& ev) {
    Who p = personWho();
    if (ev.what & evKeyDown) {
        std::string win = personKeyPlace();
        ushort m = ev.keyDown.controlKeyState;
        bool printable = ev.keyDown.textLength > 0 && !(m & (kbCtrlShift | kbAltShift)) &&
                         ev.keyDown.keyCode != kbEnter && ev.keyDown.keyCode != kbTab && ev.keyDown.keyCode != kbBack;
        if (printable) {
            std::string ch(ev.keyDown.text, ev.keyDown.textLength);
            Act* a = nullptr;
            if (mergeLast(p, "typed ", win, 2000, a)) { a->what += ch; a->ms = nowMs(); return; }
            addAct(p, "typed " + ch, win, "");
            return;
        }
        addAct(p, "key " + keyName(ev), win, "");
        return;
    }
    if (ev.what & evMouseDown) {
        bool dbl = (ev.mouse.eventFlags & meDoubleClick) != 0;
        addAct(p, std::string(dbl ? "double-click " : "click ") + where(ev.mouse.where), placeOf(ev.mouse.where), "");
        g_downAct = g_acts.empty() ? 0 : g_acts.back().n;
        g_downAt = ev.mouse.where;
        return;
    }
    if (ev.what & evMouseUp) {
        if (g_downAct && (ev.mouse.where.x != g_downAt.x || ev.mouse.where.y != g_downAt.y)) {
            for (auto it = g_acts.rbegin(); it != g_acts.rend(); ++it)
                if (it->n == g_downAct) { it->what = "drag " + where(g_downAt) + " to " + where(ev.mouse.where); break; }
        }
        g_downAct = 0;
        return;
    }
    if (ev.what & evMouseWheel) {
        const char* dir = ev.mouse.wheel == mwUp ? "up" : ev.mouse.wheel == mwDown ? "down" : ev.mouse.wheel == mwLeft ? "left" : "right";
        std::string win = placeOf(ev.mouse.where);
        std::string what = std::string("scroll ") + dir;
        Act* a = nullptr;
        if (mergeLast(p, what, win, 1500, a)) {
            int n = 1;
            size_t x = a->what.find(" x");
            if (x != std::string::npos) n = std::atoi(a->what.c_str() + x + 2);
            a->what = what + " x" + std::to_string(n + 1);
            a->ms = nowMs();
            return;
        }
        addAct(p, what, win, "");
    }
}

// ---------------------------------------------------------------- owners and window changes

struct Known {
    std::string id, type, title;
    TRect r;
};
std::map<TView*, Known> g_known;
std::map<TView*, Who> g_owner;

std::string titleOfW(TWindow* w) {
    const char* t = w->getTitle(0);
    return t ? t : "";
}

std::string rectWords(const TRect& r) {
    return std::to_string(r.a.x) + "," + std::to_string(r.a.y) + " " + std::to_string(r.b.x - r.a.x) + "x" +
           std::to_string(r.b.y - r.a.y);
}

// ---------------------------------------------------------------- hands

struct Hand {
    TOverlayCell* view = nullptr;
    int x = -1, y = -1;           // where it is drawn
    int fx = 0, fy = 0, tx = 0, ty = 0;
    bool moving = false;
    Clock::time_point t0{}, last{};
    long long startedMs = -1, arrivedMs = -1;
    int steps = 0;
    int fromX = 0, fromY = 0;
};
std::map<std::string, Hand> g_hands;
bool g_handOn = true;
int g_handMs = 150;
int g_handLingerMs = 1000;

void drawHand(Hand& h, const std::string& actor, int x, int y) {
    x = std::max(0, std::min(x, (int)TScreen::screenWidth - 1));
    y = std::max(0, std::min(y, (int)TScreen::screenHeight - 1));
    if (!h.view) {
        h.view = new TOverlayCell(TRect(x, y, x + 1, y + 1));
        h.view->text = std::string(1, actor.empty() ? 'R' : (char)std::toupper((unsigned char)actor[0]));
        h.view->attr = TColorAttr(TColorBIOS(0), TColorBIOS(14));
        TProgram::application->insert(h.view);
    } else {
        if (h.view->origin.x != x || h.view->origin.y != y) h.view->moveTo(x, y);
        if (!(h.view->state & sfVisible)) h.view->show();
        // Keep the hand in front of anything inserted since (dialogs, marks).
        if (TProgram::application->first() != h.view) h.view->makeFirst();
    }
    if (h.x != x || h.y != y) ++h.steps;
    h.x = x;
    h.y = y;
}

}  // namespace

// ---------------------------------------------------------------- who: public

const char* sourceName(Source s) {
    switch (s) {
        case Source::Console: return "console";
        case Source::Resident: return "resident";
        case Source::Guest: return "guest";
        case Source::Visitor: return "visitor";
        case Source::File: return "file";
        case Source::App: return "app";
    }
    return "app";
}

bool parseSource(const std::string& s, Source& out) {
    if (s == "console") out = Source::Console;
    else if (s == "resident") out = Source::Resident;
    else if (s == "guest") out = Source::Guest;
    else if (s == "visitor" || s == "agent") out = Source::Visitor;
    else if (s == "file") out = Source::File;
    else return false;
    return true;
}

Who currentWho() { return g_stack.empty() ? g_eventWho : g_stack.back(); }
Who personWho() { return Who{"person", Source::Console}; }

WhoScope::WhoScope(const Who& w) { g_stack.push_back(w); }
WhoScope::~WhoScope() { if (!g_stack.empty()) g_stack.pop_back(); }

void setEventWho(const Who& w) { g_eventWho = w; }

void notePut(const TEvent&) { g_putWho = currentWho(); }

void noteOrigin(const TEvent& ev, bool fromPut) {
    if (fromPut) {
        g_eventWho = g_putWho;
        if (ev.what == evCommand) {
            std::string name = commandName(ev.message.command);
            if (!name.empty()) addAct(g_putWho, "command " + name, personKeyPlace(), "");
        }
        return;
    }
    if (ev.what & (evKeyDown | evMouse)) {
        g_eventWho = personWho();
        logPerson(ev);
        return;
    }
    if (ev.what == evCommand) {
        // Not put: the status line turned a console key or click into this command.
        g_eventWho = personWho();
        std::string name = commandName(ev.message.command);
        if (!name.empty()) addAct(g_eventWho, "command " + name, personKeyPlace(), "");
        return;
    }
    g_eventWho = Who{"app", Source::App};
}

void addAct(const Who& w, const std::string& what, const std::string& window, const std::string& result, uint64_t rseq) {
    g_acts.push_back(Act{g_actN++, nowMs(), w.actor, w.source, what, window, result, rseq});
    while (g_acts.size() > kActsMax) { g_acts.pop_front(); ++g_actDropped; }
}

void updateAct(uint64_t rseq, const std::string& result) {
    if (!rseq) return;
    for (auto it = g_acts.rbegin(); it != g_acts.rend(); ++it)
        if (it->rseq == rseq) { it->result = result; return; }
}

std::string ownerName(TView* v) {
    auto it = g_owner.find(v);
    return it == g_owner.end() ? "" : it->second.actor;
}

void settle() {
    Who w = currentWho();
    auto wins = deskWindows();
    for (TWindow* win : wins) {
        auto it = g_known.find(win);
        TRect r = win->getBounds();
        if (it == g_known.end()) {
            Known k{idOfV(win), typeOfV(win), titleOfW(win), r};
            g_known[win] = k;
            if (!g_owner.count(win)) g_owner[win] = w;
            addAct(w, "opened " + k.type + " \"" + k.title + "\" at " + rectWords(r), k.id, "");
            continue;
        }
        Known& k = it->second;
        if (k.r != r) {
            bool sized = (k.r.b.x - k.r.a.x) != (r.b.x - r.a.x) || (k.r.b.y - k.r.a.y) != (r.b.y - r.a.y);
            std::string what = std::string(sized ? "resized " : "moved ") + "to " + rectWords(r);
            Act* a = nullptr;
            if (mergeLast(w, sized ? "resized " : "moved ", k.id, 3000, a)) { a->what = what; a->ms = nowMs(); }
            else addAct(w, what, k.id, "");
            k.r = r;
        }
        std::string t = titleOfW(win);
        if (t != k.title) {
            addAct(w, "retitled \"" + t + "\"", k.id, "");
            k.title = t;
        }
    }
    for (auto it = g_known.begin(); it != g_known.end();) {
        if (std::find(wins.begin(), wins.end(), (TWindow*)it->first) == wins.end()) {
            addAct(w, "closed " + it->second.type + " \"" + it->second.title + "\"", it->second.id, "");
            g_owner.erase(it->first);
            it = g_known.erase(it);
        } else {
            ++it;
        }
    }
}

// ---------------------------------------------------------------- IPC

namespace {

bool readOnly(const std::string& name, const KV& kv) {
    static const char* names[] = {"get_state", "get_canvas_size", "get_capabilities", "get_window_types", "window_text",
                                  "acts", "residents_about", "standing", "desktop_get", "terminal_read",
                                  "get_chat_history", "subscribe_events", "ping", "figlet_list_fonts",
                                  "list_figlet_fonts", "preview_figlet", "gallery_list", "paint_export", "batch",
                                  "exec_command", "resident_status", "resident_front", "resident_key",
                                  "resident_mouse", "resident_press", "resident_menu"};
    for (const char* n : names) if (name == n) return true;
    if (name.rfind("get_", 0) == 0 || name.rfind("list_", 0) == 0) return true;
    if (name == "look_mark") return !has(kv, "on") && !has(kv, "ms");
    if (name.rfind("look_", 0) == 0) return true;
    if (name == "resident_manners") return !has(kv, "share_ms") && !has(kv, "wait_on_button") && !has(kv, "clear");
    if (name == "resident_hand") return !has(kv, "on") && !has(kv, "ms");
    if (name == "window_owner") return !has(kv, "owner");
    return false;
}

std::string describeCmd(const std::string& name, const KV& kv) {
    std::string s = name;
    int n = 0;
    for (const auto& p : kv) {
        if (p.first == "id" || p.first == "actor" || p.first == "source" || p.first == "name" || p.first == "window_id")
            continue;
        if (++n > 5) { s += " ..."; break; }
        std::string v = p.second;
        if (v.size() > 24) v = v.substr(0, 21) + "...";
        s += " " + p.first + "=" + v;
    }
    return s;
}

std::string resultOf(const std::string& resp) {
    std::string r = resp;
    while (!r.empty() && (r.back() == '\n' || r.back() == '\r')) r.pop_back();
    if (r.rfind("err", 0) == 0) return r.size() > 100 ? r.substr(0, 97) + "..." : r;
    if (r.find("\"error\"") != std::string::npos) return "error: " + (r.size() > 80 ? r.substr(0, 77) + "..." : r);
    return "ok";
}

struct IpcFrame {
    std::string name;
    bool pushed;
};
std::vector<IpcFrame> g_ipc;

}  // namespace

void ipcBegin(const std::string& cmd, const KV& kv) {
    std::string name = cmd == "exec_command" ? str(kv, "name") : cmd;
    Who w;
    w.actor = str(kv, "actor");
    bool resident = name.rfind("resident_", 0) == 0;
    Source s;
    std::string claim = str(kv, "source");
    if (!claim.empty() && parseSource(claim, s) && s != Source::Console) {
        w.source = s;
    } else if (!g_ipc.empty() && g_ipc.back().name == "batch" && w.actor.empty() && claim.empty()) {
        w = currentWho();   // a command inside a batch runs as the batch's caller
    } else {
        w.source = resident ? Source::Resident : Source::Visitor;
        if (claim == "console") addAct(Who{w.actor.empty() ? "visitor" : w.actor, Source::Visitor},
                                       "claimed to be the console; treated as a visitor", "", "refused");
    }
    if (w.actor.empty() || (resident && w.actor == "api")) w.actor = resident ? "resident" : (w.source == Source::Visitor ? "visitor" : sourceName(w.source));
    g_stack.push_back(w);
    g_ipc.push_back(IpcFrame{name, true});
}

void ipcEnd(const std::string& cmd, const KV& kv, const std::string& resp) {
    std::string name = cmd == "exec_command" ? str(kv, "name") : cmd;
    if (!readOnly(name, kv)) {
        std::string win = str(kv, "id");
        if (win.empty()) win = str(kv, "window_id");
        if (win.empty()) {   // a window the command created names itself in the reply
            size_t p = resp.find("\"id\":\"");
            if (p != std::string::npos) {
                size_t e = resp.find('"', p + 6);
                if (e != std::string::npos) win = resp.substr(p + 6, e - p - 6);
            }
        }
        addAct(currentWho(), describeCmd(name, kv), win, resultOf(resp));
    }
    settle();   // what the command opened, moved or closed, after the command itself
    if (!g_ipc.empty()) g_ipc.pop_back();
    if (!g_stack.empty()) g_stack.pop_back();
}

// ---------------------------------------------------------------- hands: public

bool handReady(const std::string& actor, int x, int y) {
    if (!g_handOn) return true;
    x = std::max(0, std::min(x, (int)TScreen::screenWidth - 1));
    y = std::max(0, std::min(y, (int)TScreen::screenHeight - 1));
    Hand& h = g_hands[actor];
    auto now = Clock::now();
    h.last = now;
    if (!h.moving && h.x == x && h.y == y) {
        drawHand(h, actor, x, y);
        return true;
    }
    if (!h.moving || h.tx != x || h.ty != y) {
        h.fx = h.x < 0 ? std::max(0, x - 8) : h.x;
        h.fy = h.y < 0 ? std::max(0, y - 4) : h.y;
        h.tx = x;
        h.ty = y;
        h.t0 = now;
        h.moving = true;
        h.steps = 0;
        h.startedMs = nowMs();
        h.fromX = h.fx;
        h.fromY = h.fy;
        drawHand(h, actor, h.fx, h.fy);
        return false;
    }
    long long el = std::chrono::duration_cast<std::chrono::milliseconds>(now - h.t0).count();
    if (el >= g_handMs) {
        drawHand(h, actor, x, y);
        h.moving = false;
        h.arrivedMs = nowMs();
        return true;
    }
    double t = (double)el / (double)g_handMs;
    drawHand(h, actor, h.fx + (int)((h.tx - h.fx) * t + 0.5), h.fy + (int)((h.ty - h.fy) * t + 0.5));
    return false;
}

void handFollow(const std::string& actor, int x, int y) {
    if (!g_handOn) return;
    Hand& h = g_hands[actor];
    h.last = Clock::now();
    h.moving = false;
    h.tx = x;
    h.ty = y;
    drawHand(h, actor, x, y);
}

namespace {
void tickHands() {
    auto now = Clock::now();
    for (auto& p : g_hands) {
        Hand& h = p.second;
        if (!h.view || !(h.view->state & sfVisible) || h.moving) continue;
        if (!g_handOn || std::chrono::duration_cast<std::chrono::milliseconds>(now - h.last).count() > g_handLingerMs)
            h.view->hide();
    }
}
}  // namespace

void tickActs() { tickHands(); }

// ---------------------------------------------------------------- commands

std::string cmdActs(const KV& kv) {
    uint64_t since = (uint64_t)std::strtoull(str(kv, "since", "0").c_str(), nullptr, 10);
    int limit = std::max(1, std::min(num(kv, "limit", 500), (int)kActsMax));
    std::string actor = str(kv, "by");
    std::string win = str(kv, "window");
    std::ostringstream os;
    os << "{\"next\":" << g_actN << ",\"dropped\":" << g_actDropped << ",\"kept\":" << kActsMax << ",\"acts\":[";
    bool first = true;
    int n = 0;
    for (const auto& a : g_acts) {
        if (a.n <= since) continue;
        if (!actor.empty() && a.actor != actor) continue;
        if (!win.empty() && a.window != win) continue;
        if (++n > limit) break;
        os << (first ? "" : ",") << "{\"n\":" << a.n << ",\"ms\":" << a.ms << ",\"actor\":" << q(a.actor)
           << ",\"source\":" << q(sourceName(a.source)) << ",\"what\":" << q(a.what) << ",\"window\":" << q(a.window)
           << ",\"result\":" << q(a.result);
        if (a.rseq) os << ",\"seq\":" << a.rseq;
        os << "}";
        first = false;
    }
    os << "]}";
    return os.str();
}

std::string cmdHand(const KV& kv) {
    if (has(kv, "on")) {
        std::string v = str(kv, "on");
        g_handOn = (v == "1" || v == "true" || v == "on");
        if (!g_handOn) for (auto& p : g_hands) if (p.second.view) p.second.view->hide();
    }
    if (has(kv, "ms")) g_handMs = std::max(0, std::min(num(kv, "ms", g_handMs), 2000));
    std::ostringstream os;
    os << "{\"on\":" << (g_handOn ? "true" : "false") << ",\"ms\":" << g_handMs << ",\"linger_ms\":" << g_handLingerMs
       << ",\"hands\":[";
    bool first = true;
    for (auto& p : g_hands) {
        const Hand& h = p.second;
        os << (first ? "" : ",") << "{\"actor\":" << q(p.first) << ",\"x\":" << h.x << ",\"y\":" << h.y
           << ",\"visible\":" << ((h.view && (h.view->state & sfVisible)) ? "true" : "false")
           << ",\"moving\":" << (h.moving ? "true" : "false") << ",\"from\":[" << h.fromX << "," << h.fromY
           << "],\"to\":[" << h.tx << "," << h.ty << "],\"started_ms\":" << h.startedMs
           << ",\"arrived_ms\":" << h.arrivedMs << ",\"steps\":" << h.steps << "}";
        first = false;
    }
    os << "]}";
    return os.str();
}

std::string cmdOwner(const KV& kv) {
    std::string id = str(kv, "id");
    if (id.empty()) return "err missing id";
    TWindow* w = hooks().findWindow ? hooks().findWindow(id) : nullptr;
    if (!w) return "err window not found: " + id;
    if (has(kv, "owner")) {
        std::string o = str(kv, "owner");
        if (o.empty()) return "err empty owner";
        Source s = Source::Resident;
        if (o == "person") s = Source::Console;
        g_owner[w] = Who{o, s};
    }
    auto it = g_owner.find(w);
    std::ostringstream os;
    os << "{\"id\":" << q(id) << ",\"owner\":" << q(it == g_owner.end() ? "" : it->second.actor)
       << ",\"source\":" << q(it == g_owner.end() ? "" : sourceName(it->second.source)) << "}";
    return os.str();
}

}  // namespace resident
