// Resident input. See resident_input.h and docs/resident-input.md.
#define Uses_TApplication
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TDialog
#define Uses_TButton
#define Uses_TCluster
#define Uses_TCheckBoxes
#define Uses_TRadioButtons
#define Uses_TMultiCheckBoxes
#define Uses_TInputLine
#define Uses_TStaticText
#define Uses_TLabel
#define Uses_TListViewer
#define Uses_TListBox
#define Uses_THistory
#define Uses_TMenuView
#define Uses_TMenuBar
#define Uses_TMenuBox
#define Uses_TMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TEventQueue
#define Uses_TScrollBar
#define Uses_TFrame
#define Uses_TStringCollection
#include <tvision/tv.h>

#include "resident_input.h"
#include "resident_senses.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <set>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

// The modal view of the innermost execView loop (tgroup.cpp); 0 at the top level.
extern TView *TheTopView;

namespace resident {
namespace {

using Clock = std::chrono::steady_clock;

// Read protected members of stock controls without changing Turbo Vision.
// A pointer to member formed through a derived class is the legal route.
struct PeekButton : TButton {
    static ushort TButton::*command_() { return &PeekButton::command; }
    static uchar TButton::*flags_() { return &PeekButton::flags; }
    static Boolean TButton::*amDefault_() { return &PeekButton::amDefault; }
};
struct PeekCluster : TCluster {
    static TStringCollection* TCluster::*strings_() { return &PeekCluster::strings; }
    static uint32_t TCluster::*value_() { return &PeekCluster::value; }
};
struct PeekMenuView : TMenuView {
    static TMenu* TMenuView::*menu_() { return &PeekMenuView::menu; }
    static TMenuItem* TMenuView::*current_() { return &PeekMenuView::current; }
};
struct PeekLabel : TLabel {
    static TView* TLabel::*link_() { return &PeekLabel::link; }
};

enum class Kind { Event, Press, Command };

struct Item {
    uint64_t seq = 0;
    Kind kind = Kind::Event;
    TEvent ev{};
    TView* target = nullptr;      // nullptr: unaddressed (routed as the person's input is)
    std::string targetId;         // for the log
    std::string what;             // for the log
    TView* control = nullptr;     // Press: the control
    int index = -1;               // Press: cluster or list item
    bool hasValue = false;        // Press: text for an input line
    std::string value;
};

struct LogEntry {
    uint64_t seq;
    std::string what;
    std::string target;
    std::string result;
};

// One dispatch the resident layer is performing: who it belongs to, the view it
// went to, the modal view at its start, and the person's focus at its start.
struct Frame {
    char who;                     // 'R' resident, 'P' person
    TView* target;
    TView* topAtEntry;
    TView* personFocus;
    uint64_t serial;
};

Hooks g_hooks;
std::deque<Item> g_queue;
std::deque<LogEntry> g_log;
const size_t kLogMax = 4000;
uint64_t g_nextSeq = 1;
uint64_t g_delivered = 0;
uint64_t g_refused = 0;
uint64_t g_frameSerial = 0;
std::vector<Frame> g_frames;

// The person.
uchar g_personButtons = 0;
TView* g_personWindow = nullptr;
Clock::time_point g_personLast{};
bool g_personEver = false;
int g_shareMs = 1500;
bool g_waitOnButton = true;
std::deque<TEvent> g_heldPerson;   // person input that reached a window busy under a resident dialog

// The resident's mouse button, between its down and its up.
bool g_resDown = false;
bool g_resDownRaw = false;          // the down went into a running loop, not to an address
TView* g_resDownTarget = nullptr;
uint64_t g_resDownFrame = 0;
TPoint g_resLastWhere{0, 0};

std::set<TView*> g_owned;           // windows the resident opened

// ---------------------------------------------------------------- helpers

std::string jsonEscape(const std::string& s) {
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

std::string q(const std::string& s) { return "\"" + jsonEscape(s) + "\""; }

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Label as shown, without the hot-key tildes.
std::string plainLabel(const char* s) {
    std::string o;
    if (!s) return o;
    for (const char* p = s; *p; ++p) if (*p != '~') o += *p;
    return o;
}

// Label as matched: no tildes, trimmed, no trailing ':' or "...", lower case.
std::string matchKey(const std::string& s) {
    std::string o = lower(plainLabel(s.c_str()));
    auto trim = [](std::string& t) {
        while (!t.empty() && (t.back() == ' ' || t.back() == ':' || t.back() == '.')) t.pop_back();
        size_t i = 0;
        while (i < t.size() && t[i] == ' ') ++i;
        t.erase(0, i);
    };
    trim(o);
    return o;
}

long long msSince(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}

bool isChildOf(TGroup* g, TView* v) {
    if (!g || !v) return false;
    TView* f = g->first();
    if (!f) return false;
    TView* p = f;
    do {
        if (p == v) return true;
        p = p->next;
    } while (p != f);
    return false;
}

bool alive(TView* v) {
    return v && (isChildOf(TProgram::deskTop, v) || isChildOf(TProgram::application, v));
}

bool controlIn(TGroup* g, TView* c) { return isChildOf(g, c); }

std::string idOf(TView* v) {
    if (!v || !g_hooks.idOf) return "";
    if (auto* w = dynamic_cast<TWindow*>(v)) return g_hooks.idOf(w);
    return "";
}

std::string nameOf(TView* v) {
    if (!v) return "";
    std::string id = idOf(v);
    if (!id.empty()) return id;
    if (dynamic_cast<TMenuView*>(v)) return "menu";
    return "view";
}

TWindow* windowAt(TPoint where, TView* exclude = nullptr) {
    TDeskTop* d = TProgram::deskTop;
    if (!d) return nullptr;
    TPoint p = d->makeLocal(where);
    TView* f = d->first();
    if (!f) return nullptr;
    TView* v = f;
    do {
        if (v != exclude && (v->state & sfVisible)) {
            auto* w = dynamic_cast<TWindow*>(v);
            if (w && w->getBounds().contains(p)) return w;
        }
        v = v->next;
    } while (v != f);
    return nullptr;
}

// Where the person's keys go now: the running modal view, else the focused window.
TView* personKeyTarget() {
    if (TheTopView) return TheTopView;
    return TProgram::deskTop ? TProgram::deskTop->current : nullptr;
}

bool inFrames(TView* v) {
    for (auto& f : g_frames) if (f.target == v) return true;
    return false;
}

bool residentLoop() { return !g_frames.empty() && g_frames.back().who == 'R'; }

void addLog(uint64_t seq, const std::string& what, const std::string& target, const std::string& result) {
    g_log.push_back({seq, what, target, result});
    while (g_log.size() > kLogMax) g_log.pop_front();
}

void noteDelivered(const Item& it, const std::string& how) {
    ++g_delivered;
    addLog(it.seq, it.what, it.targetId, how);
}

// Change the result of an entry already in the log (newest first).
void updateLog(uint64_t seq, const std::string& result) {
    for (auto it = g_log.rbegin(); it != g_log.rend(); ++it)
        if (it->seq == seq) { it->result = result; return; }
}

void noteRefused(const Item& it, const std::string& why) {
    ++g_refused;
    addLog(it.seq, it.what, it.targetId, "refused: " + why);
}

// The person is acting in this window: their button is down in it, or they gave it
// input within the last share_ms.
bool personActingIn(TView* w) {
    if (!w || !g_personEver) return false;
    bool recent = msSince(g_personLast) < g_shareMs;
    // A dialog the person's input opened is theirs while they are active.
    if (w == TheTopView && !g_owned.count(w) && recent && !residentLoop()) return true;
    if (w != g_personWindow) return false;
    if (g_personButtons) return true;
    return recent;
}

// Where an item would land (for the sharing check). A menu command without an
// address is the application's, not a window's: it is not checked.
TView* effectiveTarget(const Item& it) {
    if (it.target) return it.target;
    if (it.kind == Kind::Command) return nullptr;
    if (it.ev.what & evMouse) return windowAt(it.ev.mouse.where);
    return personKeyTarget();
}

void observePerson(const TEvent& ev) {
    auto now = Clock::now();
    if (ev.what & evMouseDown) {
        g_personButtons = ev.mouse.buttons ? ev.mouse.buttons : 1;
        g_personWindow = windowAt(ev.mouse.where, residentLoop() ? TheTopView : nullptr);
        g_personLast = now;
        g_personEver = true;
    } else if (ev.what & evMouseUp) {
        g_personButtons = 0;
        g_personLast = now;
    } else if (ev.what & (evMouseMove | evMouseAuto)) {
        g_personButtons = ev.mouse.buttons;
        if (ev.mouse.buttons) g_personLast = now;
    } else if (ev.what & evMouseWheel) {
        g_personWindow = windowAt(ev.mouse.where);
        g_personLast = now;
        g_personEver = true;
    } else if (ev.what & evKeyDown) {
        // During a resident dialog the person's keys go to their own window (routePerson).
        g_personWindow = residentLoop() ? g_frames.back().personFocus : personKeyTarget();
        g_personLast = now;
        g_personEver = true;
    }
}

// ---------------------------------------------------------------- dispatch

std::string pressControl(Item& it) {
    TView* c = it.control;
    auto* g = dynamic_cast<TGroup*>(it.target);
    if (!g || !controlIn(g, c)) return "refused: control gone";
    if (c->state & sfDisabled) return "refused: control disabled";
    if (auto* b = dynamic_cast<TButton*>(c)) {
        if (!TView::commandEnabled(b->*PeekButton::command_())) return "refused: command disabled";
        b->press();
        return "pressed";
    }
    if (auto* cl = dynamic_cast<TCluster*>(c)) {
        if (it.index < 0) return "refused: no item";
        cl->press(it.index);
        cl->drawView();
        return "pressed item " + std::to_string(it.index);
    }
    if (auto* in = dynamic_cast<TInputLine*>(c)) {
        if (it.hasValue) {
            std::vector<char> buf(in->dataSize() + 1, '\0');
            std::strncpy(buf.data(), it.value.c_str(), buf.size() - 1);
            in->setData(buf.data());
            in->drawView();
            return "set";
        }
        in->select();
        return "selected";
    }
    if (auto* lv = dynamic_cast<TListViewer*>(c)) {
        if (it.index < 0 || it.index >= lv->range) return "refused: no item";
        lv->focusItemNum((short)it.index);
        lv->selectItem((short)it.index);
        return "selected item " + std::to_string(it.index);
    }
    return "refused: cannot press this control";
}

void collectWindows(std::vector<TView*>& out) {
    out.clear();
    TDeskTop* d = TProgram::deskTop;
    if (!d || !d->first()) return;
    TView* f = d->first();
    TView* v = f;
    do {
        if (dynamic_cast<TWindow*>(v)) out.push_back(v);
        v = v->next;
    } while (v != f);
}

// Hand an item to its window (or to the application) directly, without the
// person's focus chain. The person's focused window is restored afterwards.
void deliverAddressed(Item& it) {
    TDeskTop* d = TProgram::deskTop;
    Frame f{'R', it.target, TheTopView, d ? d->current : nullptr, ++g_frameSerial};
    std::vector<TView*> before;
    collectWindows(before);
    // Logged before the dispatch: a dialog the input opens runs inside it.
    noteDelivered(it, "delivered");
    g_frames.push_back(f);
    std::string result = "delivered";

    if (it.kind == Kind::Press) {
        result = pressControl(it);
    } else {
        TEvent e = it.ev;
        TView* t = it.target ? it.target : TProgram::application;
        auto* w = dynamic_cast<TWindow*>(t);
        bool fakeSel = false, fakeFrame = false;
        if (e.what & evMouseDown) {
            g_resDown = true;
            g_resDownRaw = false;
            g_resDownTarget = it.target;
            g_resDownFrame = f.serial;
            g_resLastWhere = e.mouse.where;
            // A click must not select (and so raise and focus) the window.
            if (w && !(w->state & sfSelected)) {
                w->state |= (sfSelected | sfActive);
                fakeSel = true;
                if (w->frame && !(w->frame->state & sfActive)) { w->frame->state |= sfActive; fakeFrame = true; }
            }
        } else if (e.what & evMouseUp) {
            g_resDown = false;
        }
        if (e.what & evMouse) g_resLastWhere = e.mouse.where;
        t->handleEvent(e);
        if (it.kind == Kind::Command && it.target && e.what == evCommand) {
            // Not handled by the window: offer it to the application's own commands.
            TProgram::application->handleEvent(e);
        }
        if ((fakeSel || fakeFrame) && alive(w)) {
            if (fakeSel) w->state &= ~(sfSelected | sfActive);
            if (fakeFrame && w->frame) { w->frame->state &= ~sfActive; w->frame->drawView(); }
        }
        if (e.what != evNothing && it.kind == Kind::Event && (it.ev.what & evKeyDown)) result = "delivered (not handled)";
    }

    f.personFocus = g_frames.back().personFocus;   // the person may have clicked elsewhere meanwhile
    g_frames.pop_back();

    // Windows that appeared during the dispatch are the resident's.
    std::vector<TView*> after;
    collectWindows(after);
    for (TView* v : after)
        if (std::find(before.begin(), before.end(), v) == before.end()) g_owned.insert(v);
    // The person's focus does not move because of resident input.
    if (d && f.personFocus && d->current != f.personFocus && alive(f.personFocus) &&
        isChildOf(d, f.personFocus) && TheTopView == f.topAtEntry) {
        d->setCurrent(f.personFocus, TView::normalSelect);
    }
    if (result.rfind("refused", 0) == 0) {
        --g_delivered;
        ++g_refused;
        updateLog(it.seq, result);
    } else if (result != "delivered") {
        updateLog(it.seq, result);
    }
}

// Give an item to the loop that is running (the person's main loop for
// unaddressed input, or a resident dialog's loop).
void deliverRaw(Item& it, TEvent& ev) {
    ev = it.ev;
    if (!it.target && TProgram::statusLine &&
        ((ev.what & evKeyDown) ||
         ((ev.what & evMouseDown) && TProgram::statusLine->getBounds().contains(ev.mouse.where))))
        TProgram::statusLine->handleEvent(ev);   // status-line hot keys, as TProgram::getEvent does
    if (it.ev.what & evMouseDown) {
        g_resDown = true;
        g_resDownRaw = true;
        g_resDownTarget = nullptr;
    } else if (it.ev.what & evMouseUp) {
        g_resDown = false;
    }
    if (it.ev.what & evMouse) g_resLastWhere = it.ev.mouse.where;
    noteDelivered(it, "delivered");
}

// First item that can go now; items for a window keep their order.
std::deque<Item>::iterator pickDeliverable() {
    for (auto it = g_queue.begin(); it != g_queue.end();) {
        if (it->target && !alive(it->target)) {
            noteRefused(*it, "window gone");
            it = g_queue.erase(it);
            continue;
        }
        if (it->target && inFrames(it->target)) { ++it; continue; }   // busy under a dialog it opened
        return it;
    }
    return g_queue.end();
}

// Send the person's input to the person's window while a resident dialog runs.
void routePerson(TEvent& ev) {
    Frame& rf = g_frames.back();
    TView* tgt = nullptr;
    if (ev.what & evKeyDown) tgt = alive(rf.personFocus) ? rf.personFocus : nullptr;
    else if (ev.what & (evMouseDown | evMouseWheel)) tgt = windowAt(ev.mouse.where, TheTopView);
    else return;   // moves and ups outside a person's drag
    if (!tgt) return;
    // A click chooses where the person's keys go next, as it would without the dialog.
    if ((ev.what & evMouseDown) && dynamic_cast<TWindow*>(tgt)) rf.personFocus = tgt;
    if (inFrames(tgt)) { g_heldPerson.push_back(ev); return; }
    TDeskTop* d = TProgram::deskTop;
    Frame f{'P', tgt, TheTopView, d ? d->current : nullptr, ++g_frameSerial};
    g_frames.push_back(f);
    auto* w = dynamic_cast<TWindow*>(tgt);
    bool fake = false;
    if ((ev.what & evMouseDown) && w && !(w->state & sfSelected)) { w->state |= sfSelected; fake = true; }
    TEvent e = ev;
    tgt->handleEvent(e);
    if (fake && alive(w)) w->state &= ~sfSelected;
    g_frames.pop_back();
}

}  // namespace

// ---------------------------------------------------------------- loop hooks

void setHooks(const Hooks& h) { g_hooks = h; }
const Hooks& hooks() { return g_hooks; }

std::string ownerOf(TView* v) { return g_owned.count(v) ? "resident" : "person"; }

bool takeEvent(TEvent& ev) {
    // 1. Between the resident's mouse down and up, its own pointer events come first.
    if (g_resDown) {
        auto it = g_queue.begin();
        for (; it != g_queue.end(); ++it)
            if (it->kind == Kind::Event && (it->ev.what & evMouse) && (g_resDownRaw || it->target == g_resDownTarget)) break;
        Item item;
        if (it != g_queue.end()) {
            item = *it;
            g_queue.erase(it);
        } else {
            item.seq = 0;
            item.ev.what = evMouseUp;
            item.ev.mouse.where = g_resLastWhere;
            item.ev.mouse.buttons = 0;
            item.target = g_resDownRaw ? nullptr : g_resDownTarget;
            item.targetId = item.target ? nameOf(item.target) : "";
            item.what = "mouse up (added: a down needs an up)";
        }
        bool inside = false;
        for (auto& f : g_frames) if (f.serial == g_resDownFrame) inside = true;
        if (g_resDownRaw || inside) {
            ev = item.ev;
            if (item.ev.what & evMouseUp) g_resDown = false;
            g_resLastWhere = item.ev.mouse.where;
            noteDelivered(item, "delivered");
            return true;
        }
        if (!alive(g_resDownTarget)) { g_resDown = false; ev.what = evNothing; return true; }
        deliverAddressed(item);
        ev.what = evNothing;
        return true;
    }

    // 2. A loop the resident started (a dialog its input opened): its items go first,
    //    the person's input is routed in afterBase.
    if (residentLoop()) {
        Frame& top = g_frames.back();
        if (TheTopView && TheTopView != top.topAtEntry) g_owned.insert(TheTopView);
        if (!(g_waitOnButton && g_personButtons)) {
            auto it = pickDeliverable();
            if (it != g_queue.end()) {
                Item item = *it;
                g_queue.erase(it);
                if (personActingIn(effectiveTarget(item))) {
                    noteRefused(item, "Sharing violation");
                    ev.what = evNothing;
                    return true;
                }
                if (item.kind == Kind::Event && (!item.target || item.target == TheTopView)) {
                    deliverRaw(item, ev);
                    return true;
                }
                deliverAddressed(item);
                ev.what = evNothing;
                return true;
            }
        }
        return false;
    }

    // 3. Person input that waited while a window was busy under a resident dialog.
    if (!g_heldPerson.empty()) {
        ev = g_heldPerson.front();
        g_heldPerson.pop_front();
        return true;
    }

    // 4. Work is ready: do not let the base getEvent sleep its 20 ms.
    if (!g_queue.empty() && !(g_waitOnButton && g_personButtons)) TEventQueue::wakeUp();
    return false;
}

void afterBase(TEvent& ev) {
    tickSenses();
    if (ev.what & (evMouse | evKeyDown)) {
        observePerson(ev);
        if (residentLoop()) {
            routePerson(ev);
            ev.what = evNothing;
        }
        return;
    }
    if (ev.what != evNothing) return;              // a put event passes through
    if (g_waitOnButton && g_personButtons) return; // manners: wait for the person's button
    if (residentLoop()) return;
    auto it = pickDeliverable();
    if (it == g_queue.end()) return;
    Item item = *it;
    g_queue.erase(it);
    if (personActingIn(effectiveTarget(item))) {
        noteRefused(item, "Sharing violation");
        return;
    }
    if (item.kind == Kind::Event && !item.target) {
        deliverRaw(item, ev);
        return;
    }
    deliverAddressed(item);
}

bool interceptPut(TEvent& ev) {
    if (g_frames.empty()) return false;
    Frame& f = g_frames.back();
    if (f.who != 'R' || !f.target) return false;
    if (TheTopView != f.topAtEntry) return false;   // a dialog opened inside: its loop takes its own puts
    if (!(ev.what & (evCommand | evBroadcast | evKeyDown))) return false;
    Item it;
    it.seq = g_nextSeq++;
    it.kind = Kind::Command;
    it.ev = ev;
    it.target = f.target;
    it.targetId = nameOf(f.target);
    it.what = (ev.what & evKeyDown) ? "put key" : ("put command " + std::to_string(ev.message.command));
    if (ev.what & evKeyDown) it.kind = Kind::Event;
    g_queue.push_front(it);
    return true;
}

// ---------------------------------------------------------------- keys

namespace {

struct Named { const char* name; ushort plain, shift, ctrl, alt; };
const Named kNamed[] = {
    {"enter", kbEnter, kbEnter, kbCtrlEnter, kbAltEnter},
    {"return", kbEnter, kbEnter, kbCtrlEnter, kbAltEnter},
    {"esc", kbEsc, kbEsc, kbEsc, kbAltEsc},
    {"escape", kbEsc, kbEsc, kbEsc, kbAltEsc},
    {"tab", kbTab, kbShiftTab, kbCtrlTab, kbAltTab},
    {"backspace", kbBack, kbBack, kbCtrlBack, kbAltBack},
    {"back", kbBack, kbBack, kbCtrlBack, kbAltBack},
    {"bs", kbBack, kbBack, kbCtrlBack, kbAltBack},
    {"space", ' ', ' ', ' ', kbAltSpace},
    {"up", kbUp, kbUp, kbCtrlUp, kbAltUp},
    {"down", kbDown, kbDown, kbCtrlDown, kbAltDown},
    {"left", kbLeft, kbLeft, kbCtrlLeft, kbAltLeft},
    {"right", kbRight, kbRight, kbCtrlRight, kbAltRight},
    {"home", kbHome, kbHome, kbCtrlHome, kbAltHome},
    {"end", kbEnd, kbEnd, kbCtrlEnd, kbAltEnd},
    {"pgup", kbPgUp, kbPgUp, kbCtrlPgUp, kbAltPgUp},
    {"pageup", kbPgUp, kbPgUp, kbCtrlPgUp, kbAltPgUp},
    {"pgdn", kbPgDn, kbPgDn, kbCtrlPgDn, kbAltPgDn},
    {"pagedown", kbPgDn, kbPgDn, kbCtrlPgDn, kbAltPgDn},
    {"ins", kbIns, kbShiftIns, kbCtrlIns, kbAltIns},
    {"insert", kbIns, kbShiftIns, kbCtrlIns, kbAltIns},
    {"del", kbDel, kbShiftDel, kbCtrlDel, kbAltDel},
    {"delete", kbDel, kbShiftDel, kbCtrlDel, kbAltDel},
};

ushort altLetter(char c) {
    static const char* row = "QWERTYUIOP";     // scan 0x10..0x19
    static const char* row2 = "ASDFGHJKL";     // 0x1e..0x26
    static const char* row3 = "ZXCVBNM";       // 0x2c..0x32
    c = (char)std::toupper((unsigned char)c);
    if (const char* p = std::strchr(row, c)) return (ushort)((0x10 + (p - row)) << 8);
    if (const char* p = std::strchr(row2, c)) return (ushort)((0x1e + (p - row2)) << 8);
    if (const char* p = std::strchr(row3, c)) return (ushort)((0x2c + (p - row3)) << 8);
    return 0;
}

size_t utf8Len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xe) return 3;
    if ((c >> 3) == 0x1e) return 4;
    return 1;
}

void setText(TEvent& e, const std::string& s) {
    size_t n = std::min(s.size(), sizeof(e.keyDown.text));
    std::memcpy(e.keyDown.text, s.data(), n);
    e.keyDown.textLength = (uchar)n;
}

TEvent keyEvent(ushort code, ushort mods) {
    TEvent e{};
    e.what = evKeyDown;
    e.keyDown.keyCode = code;
    e.keyDown.controlKeyState = mods;
    e.keyDown.textLength = 0;
    return e;
}

// One printable character (one UTF-8 code point), with optional modifiers.
bool charKey(const std::string& ch, ushort mods, TEvent& out) {
    unsigned char c0 = (unsigned char)ch[0];
    bool ascii = ch.size() == 1 && c0 < 0x80;
    if (ascii && (mods & kbAltShift)) {
        char c = (char)c0;
        ushort code = 0;
        if (std::isalpha((unsigned char)c)) code = altLetter(c);
        else if (c >= '1' && c <= '9') code = (ushort)(kbAlt1 + ((c - '1') << 8));
        else if (c == '0') code = kbAlt0;
        else if (c == '-') code = kbAltMinus;
        else if (c == '=') code = kbAltEqual;
        if (!code) return false;
        out = keyEvent(code, mods);
        return true;
    }
    if (ascii && (mods & kbCtrlShift)) {
        char c = (char)std::toupper(c0);
        if (c < 'A' || c > 'Z') return false;
        out = keyEvent((ushort)(c - 'A' + 1), mods);
        return true;
    }
    std::string s = ch;
    if (ascii && (mods & kbShift) && std::isalpha(c0)) s[0] = (char)std::toupper(c0);
    out = keyEvent(ascii ? (ushort)(unsigned char)s[0] : 0, mods);
    setText(out, s);
    return true;
}

bool parseKeyToken(const std::string& tok, TEvent& out) {
    ushort mods = 0;
    std::string rest = tok;
    for (;;) {
        size_t p = rest.find('+');
        if (p == std::string::npos || p == 0 || p + 1 == rest.size()) break;
        std::string m = lower(rest.substr(0, p));
        if (m == "ctrl" || m == "control") mods |= kbCtrlShift;
        else if (m == "alt" || m == "meta") mods |= kbAltShift;
        else if (m == "shift") mods |= kbShift;
        else break;
        rest = rest.substr(p + 1);
    }
    if (rest.empty()) return false;
    if (rest.size() > 2 && rest[0] == '0' && (rest[1] == 'x' || rest[1] == 'X')) {
        out = keyEvent((ushort)std::strtoul(rest.c_str(), nullptr, 16), mods);
        return true;
    }
    std::string lr = lower(rest);
    for (const auto& n : kNamed) {
        if (lr != n.name) continue;
        ushort code = (mods & kbAltShift) ? n.alt : (mods & kbCtrlShift) ? n.ctrl : (mods & kbShift) ? n.shift : n.plain;
        out = keyEvent(code, mods);
        if (code == ' ') setText(out, " ");
        return true;
    }
    if ((lr[0] == 'f') && lr.size() >= 2 && lr.size() <= 3 && std::isdigit((unsigned char)lr[1])) {
        int k = std::atoi(lr.c_str() + 1);
        if (k >= 1 && k <= 12) {
            ushort code;
            int i = k - 1;
            if (mods & kbAltShift) code = k <= 10 ? (ushort)(kbAltF1 + (i << 8)) : (k == 11 ? kbAltF11 : kbAltF12);
            else if (mods & kbCtrlShift) code = k <= 10 ? (ushort)(kbCtrlF1 + (i << 8)) : (k == 11 ? kbCtrlF11 : kbCtrlF12);
            else if (mods & kbShift) code = k <= 10 ? (ushort)(kbShiftF1 + (i << 8)) : (k == 11 ? kbShiftF11 : kbShiftF12);
            else code = k <= 10 ? (ushort)(kbF1 + (i << 8)) : (k == 11 ? kbF11 : kbF12);
            out = keyEvent(code, mods);
            return true;
        }
    }
    if (utf8Len((unsigned char)rest[0]) == rest.size()) return charKey(rest, mods, out);
    return false;
}

std::vector<std::string> splitSpaces(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream is(s);
    std::string t;
    while (is >> t) out.push_back(t);
    return out;
}

std::string str(const KV& kv, const char* k, const std::string& def = "") {
    auto it = kv.find(k);
    return it == kv.end() ? def : it->second;
}

bool has(const KV& kv, const char* k) { return kv.find(k) != kv.end(); }

int num(const KV& kv, const char* k, int def) {
    auto it = kv.find(k);
    return it == kv.end() || it->second.empty() ? def : std::atoi(it->second.c_str());
}

// Resolve id= to a window; "" when absent. err set when given but unknown.
TView* resolveTarget(const KV& kv, std::string& err) {
    std::string id = str(kv, "id");
    if (id.empty()) return nullptr;
    TWindow* w = g_hooks.findWindow ? g_hooks.findWindow(id) : nullptr;
    if (!w) err = "err window not found: " + id;
    return w;
}

std::string enqueueAll(std::vector<Item>& items) {
    if (items.empty()) return "err nothing to send";
    for (auto& it : items) {
        it.seq = g_nextSeq++;
        g_queue.push_back(it);
    }
    TEventQueue::wakeUp();
    std::ostringstream os;
    os << "{\"ok\":true,\"first\":" << items.front().seq << ",\"last\":" << items.back().seq
       << ",\"count\":" << items.size() << ",\"queued\":" << g_queue.size() << "}";
    return os.str();
}

std::string sharingError(TView* w) {
    return "err Sharing violation: the person is acting in " + nameOf(w);
}

}  // namespace

std::string cmdKey(const KV& kv) {
    std::string err;
    TView* target = resolveTarget(kv, err);
    if (!err.empty()) return err;
    std::vector<Item> items;
    std::string id = target ? nameOf(target) : "";
    if (has(kv, "keys")) {
        for (const auto& tok : splitSpaces(str(kv, "keys"))) {
            Item it;
            if (!parseKeyToken(tok, it.ev)) return "err unknown key: " + tok;
            it.target = target;
            it.targetId = id;
            it.what = "key " + tok;
            items.push_back(it);
        }
    }
    if (has(kv, "text")) {
        std::string t = str(kv, "text");
        for (size_t i = 0; i < t.size();) {
            size_t n = utf8Len((unsigned char)t[i]);
            std::string ch = t.substr(i, n);
            i += n;
            Item it;
            if (ch == "\r") continue;
            if (ch == "\n") { it.ev = keyEvent(kbEnter, 0); it.what = "key Enter"; }
            else if (ch == "\t") { it.ev = keyEvent(kbTab, 0); it.what = "key Tab"; }
            else { charKey(ch, 0, it.ev); it.what = "key " + ch; }
            it.target = target;
            it.targetId = id;
            items.push_back(it);
        }
    }
    if (items.empty()) return "err missing keys or text";
    TView* lands = target ? target : personKeyTarget();
    if (personActingIn(lands)) return sharingError(lands);
    return enqueueAll(items);
}

std::string cmdMouse(const KV& kv) {
    std::string err;
    TView* target = resolveTarget(kv, err);
    if (!err.empty()) return err;
    std::string action = lower(str(kv, "action", "click"));
    if (!has(kv, "x") || !has(kv, "y")) return "err missing x/y";
    TDeskTop* d = TProgram::deskTop;
    TPoint base{0, 0};
    if (str(kv, "screen") == "1") base = {0, 0};
    else if (target && str(kv, "local") == "1") base = {d->origin.x + target->origin.x, d->origin.y + target->origin.y};
    else base = d->origin;
    TPoint p0{base.x + num(kv, "x", 0), base.y + num(kv, "y", 0)};
    TPoint p1{base.x + num(kv, "x2", num(kv, "x", 0)), base.y + num(kv, "y2", num(kv, "y", 0))};
    std::string b = lower(str(kv, "button", "left"));
    uchar buttons = b == "right" ? mbRightButton : b == "middle" ? 0x04 : mbLeftButton;
    ushort mods = 0;
    std::string m = lower(str(kv, "mods"));
    if (m.find("ctrl") != std::string::npos) mods |= kbCtrlShift;
    if (m.find("shift") != std::string::npos) mods |= kbShift;
    if (m.find("alt") != std::string::npos) mods |= kbAltShift;

    if (target) {
        TPoint local = d->makeLocal(p0);
        if (!target->getBounds().contains(local)) return "err point outside window " + nameOf(target);
    }
    TView* lands = target ? target : windowAt(p0);
    if (personActingIn(lands)) return sharingError(lands);

    std::vector<Item> items;
    std::string id = target ? nameOf(target) : "";
    auto add = [&](ushort what, TPoint where, uchar btn, ushort flags, uchar wheel, const std::string& desc) {
        Item it;
        it.ev = TEvent{};
        it.ev.what = what;
        it.ev.mouse.where = where;
        it.ev.mouse.buttons = btn;
        it.ev.mouse.eventFlags = flags;
        it.ev.mouse.controlKeyState = mods;
        it.ev.mouse.wheel = wheel;
        it.target = target;
        it.targetId = id;
        it.what = desc + " " + std::to_string(where.x - base.x) + "," + std::to_string(where.y - base.y);
        items.push_back(it);
    };
    if (action == "click" || action == "double") {
        add(evMouseDown, p0, buttons, 0, 0, "mouse down");
        add(evMouseUp, p0, 0, 0, 0, "mouse up");
        if (action == "double" || str(kv, "double") == "1") {
            add(evMouseDown, p0, buttons, meDoubleClick, 0, "mouse down (double)");
            add(evMouseUp, p0, 0, 0, 0, "mouse up");
        }
    } else if (action == "down") {
        add(evMouseDown, p0, buttons, 0, 0, "mouse down");
    } else if (action == "up") {
        add(evMouseUp, p0, 0, 0, 0, "mouse up");
    } else if (action == "move") {
        add(evMouseMove, p0, 0, meMouseMoved, 0, "mouse move");
    } else if (action == "drag") {
        if (!has(kv, "x2") || !has(kv, "y2")) return "err drag needs x2/y2";
        int dx = p1.x - p0.x, dy = p1.y - p0.y;
        int steps = num(kv, "steps", std::max(std::abs(dx), std::abs(dy)));
        steps = std::max(1, std::min(steps, 400));
        add(evMouseDown, p0, buttons, 0, 0, "mouse down");
        for (int i = 1; i <= steps; ++i) {
            TPoint pi{p0.x + dx * i / steps, p0.y + dy * i / steps};
            add(evMouseMove, pi, buttons, meMouseMoved, 0, "mouse drag");
        }
        add(evMouseUp, p1, 0, 0, 0, "mouse up");
    } else if (action == "scroll") {
        std::string dir = lower(str(kv, "dir", "down"));
        uchar wheel = dir == "up" ? mwUp : dir == "left" ? mwLeft : dir == "right" ? mwRight : mwDown;
        int count = std::max(1, std::min(num(kv, "count", 1), 200));
        for (int i = 0; i < count; ++i) add(evMouseWheel, p0, 0, 0, wheel, "scroll " + dir);
    } else {
        return "err unknown action: " + action;
    }
    return enqueueAll(items);
}

std::string cmdManners(const KV& kv) {
    if (has(kv, "share_ms")) g_shareMs = std::max(0, num(kv, "share_ms", g_shareMs));
    if (has(kv, "wait_on_button")) g_waitOnButton = num(kv, "wait_on_button", 1) != 0;
    if (str(kv, "clear") == "1") {
        for (auto& it : g_queue) noteRefused(it, "cleared");
        g_queue.clear();
    }
    std::ostringstream os;
    os << "{\"share_ms\":" << g_shareMs << ",\"wait_on_button\":" << (g_waitOnButton ? "true" : "false") << "}";
    return os.str();
}

std::string cmdStatus(const KV& kv) {
    uint64_t since = (uint64_t)std::strtoull(str(kv, "since", "0").c_str(), nullptr, 10);
    int limit = std::max(1, std::min(num(kv, "limit", 400), 4000));
    std::ostringstream os;
    os << "{\"queued\":" << g_queue.size() << ",\"delivered\":" << g_delivered << ",\"refused\":" << g_refused
       << ",\"next_seq\":" << g_nextSeq
       << ",\"waiting\":" << q(g_waitOnButton && g_personButtons ? "person mouse button down" : "")
       << ",\"resident_button_down\":" << (g_resDown ? "true" : "false")
       << ",\"person\":{\"window\":" << q(alive(g_personWindow) ? nameOf(g_personWindow) : "")
       << ",\"buttons\":" << (int)g_personButtons
       << ",\"idle_ms\":" << (g_personEver ? msSince(g_personLast) : -1)
       << ",\"focus\":" << q(nameOf(personKeyTarget())) << "}"
       << ",\"share_ms\":" << g_shareMs << ",\"owned\":[";
    bool first = true;
    for (auto it = g_owned.begin(); it != g_owned.end();) {
        if (!alive(*it)) { it = g_owned.erase(it); continue; }
        os << (first ? "" : ",") << q(nameOf(*it));
        first = false;
        ++it;
    }
    os << "],\"log\":[";
    first = true;
    int n = 0;
    for (const auto& e : g_log) {
        if (e.seq <= since && e.seq != 0) continue;
        if (since && e.seq == 0) continue;
        if (++n > limit) break;
        os << (first ? "" : ",") << "{\"seq\":" << e.seq << ",\"what\":" << q(e.what)
           << ",\"target\":" << q(e.target) << ",\"result\":" << q(e.result) << "}";
        first = false;
    }
    os << "]}";
    return os.str();
}

// ---------------------------------------------------------------- dialogs and menus

namespace {

std::vector<TView*> childrenInOrder(TGroup* g) {
    std::vector<TView*> v;
    if (!g || !g->first()) return v;
    TView* f = g->first();
    TView* p = f;
    do { v.push_back(p); p = p->next; } while (p != f);
    std::reverse(v.begin(), v.end());   // insertion (tab) order
    return v;
}

std::map<TView*, std::string> labelsOf(TGroup* g) {
    std::map<TView*, std::string> m;
    for (TView* c : childrenInOrder(g)) {
        if (auto* l = dynamic_cast<TLabel*>(c)) {
            TView* link = l->*PeekLabel::link_();
            char buf[256] = {0};
            l->getText(buf);
            if (link) m[link] = plainLabel(buf);
        }
    }
    return m;
}

std::vector<std::string> clusterItems(TCluster* cl) {
    std::vector<std::string> out;
    TStringCollection* s = cl->*PeekCluster::strings_();
    if (!s) return out;
    for (int i = 0; i < s->getCount(); ++i) out.push_back(plainLabel((const char*)s->at(i)));
    return out;
}

std::string listItemText(TListViewer* lv, int i) {
    char buf[256] = {0};
    lv->getText(buf, (short)i, 255);
    return buf;
}

std::string menuItemsJson(TMenu* menu, int depth) {
    std::ostringstream os;
    os << "[";
    bool first = true;
    for (TMenuItem* it = menu ? menu->items : nullptr; it; it = it->next) {
        if (!it->name) continue;   // separator
        os << (first ? "" : ",") << "{\"name\":" << q(plainLabel(it->name));
        if (it->command == 0) {
            os << ",\"items\":" << (depth < 6 && it->subMenu ? menuItemsJson(it->subMenu, depth + 1) : "[]");
        } else {
            bool en = !it->disabled && TView::commandEnabled(it->command);
            os << ",\"command\":" << it->command << ",\"enabled\":" << (en ? "true" : "false");
            if (it->param) os << ",\"key\":" << q(it->param);
        }
        os << "}";
        first = false;
    }
    os << "]";
    return os.str();
}

std::string describe(TView* v) {
    if (!v) return "null";
    std::ostringstream os;
    if (auto* mv = dynamic_cast<TMenuView*>(v)) {
        TMenu* menu = mv->*PeekMenuView::menu_();
        TMenuItem* cur = mv->*PeekMenuView::current_();
        os << "{\"kind\":\"menu\",\"items\":" << menuItemsJson(menu, 5)
           << ",\"current\":" << q(cur && cur->name ? plainLabel(cur->name) : "")
           << ",\"accept\":{\"how\":\"resident_menu choose=<name>\",\"key\":\"Enter\"}"
           << ",\"cancel\":{\"key\":\"Esc\"}}";
        return os.str();
    }
    auto* g = dynamic_cast<TGroup*>(v);
    auto* w = dynamic_cast<TWindow*>(v);
    const char* kind = dynamic_cast<TDialog*>(v) ? "dialog" : w ? "window" : "view";
    os << "{\"kind\":\"" << kind << "\",\"id\":" << q(idOf(v))
       << ",\"title\":" << q(w && w->title ? w->title : "")
       << ",\"owner\":" << q(g_owned.count(v) ? "resident" : "person")
       << ",\"modal\":" << ((v->state & sfModal) ? "true" : "false");
    if (!g) { os << "}"; return os.str(); }
    auto labels = labelsOf(g);
    std::string accept, cancel;
    os << ",\"controls\":[";
    bool first = true;
    for (TView* c : childrenInOrder(g)) {
        std::ostringstream cs;
        std::string label = labels.count(c) ? labels[c] : "";
        bool en = !(c->state & sfDisabled);
        if (auto* b = dynamic_cast<TButton*>(c)) {
            ushort cmd = b->*PeekButton::command_();
            bool def = b->*PeekButton::amDefault_();
            en = en && TView::commandEnabled(cmd);
            std::string t = plainLabel(b->title);
            cs << "{\"kind\":\"button\",\"label\":" << q(t) << ",\"command\":" << cmd
               << ",\"default\":" << (def ? "true" : "false");
            if (accept.empty() && (def || cmd == cmOK || cmd == cmYes)) accept = t;
            if (cancel.empty() && (cmd == cmCancel || cmd == cmNo)) cancel = t;
        } else if (auto* cl = dynamic_cast<TCluster*>(c)) {
            const char* k = dynamic_cast<TRadioButtons*>(c) ? "radio" : dynamic_cast<TCheckBoxes*>(c) ? "checkboxes" : "cluster";
            cs << "{\"kind\":\"" << k << "\",\"label\":" << q(label) << ",\"items\":[";
            auto items = clusterItems(cl);
            for (size_t i = 0; i < items.size(); ++i)
                cs << (i ? "," : "") << "{\"label\":" << q(items[i]) << ",\"on\":" << (cl->mark((int)i) ? "true" : "false") << "}";
            cs << "]";
        } else if (auto* in = dynamic_cast<TInputLine*>(c)) {
            cs << "{\"kind\":\"input\",\"label\":" << q(label) << ",\"value\":" << q(in->data ? in->data : "");
        } else if (auto* lv = dynamic_cast<TListViewer*>(c)) {
            cs << "{\"kind\":\"list\",\"label\":" << q(label) << ",\"count\":" << lv->range
               << ",\"focused\":" << lv->focused << ",\"items\":[";
            int n = std::min<int>(lv->range, 50);
            for (int i = 0; i < n; ++i) cs << (i ? "," : "") << q(listItemText(lv, i));
            cs << "]";
        } else if (dynamic_cast<TLabel*>(c) || dynamic_cast<TFrame*>(c) || dynamic_cast<TScrollBar*>(c) ||
                   dynamic_cast<THistory*>(c)) {
            continue;
        } else if (auto* st = dynamic_cast<TStaticText*>(c)) {
            char buf[256] = {0};
            st->getText(buf);
            cs << "{\"kind\":\"text\",\"text\":" << q(buf);
        } else {
            cs << "{\"kind\":\"view\",\"class\":" << q(typeid(*c).name());
        }
        cs << ",\"enabled\":" << (en ? "true" : "false") << "}";
        os << (first ? "" : ",") << cs.str();
        first = false;
    }
    os << "]";
    if (!accept.empty()) os << ",\"accept\":{\"press\":" << q(accept) << ",\"key\":\"Enter\"}";
    if (!cancel.empty() || dynamic_cast<TDialog*>(v))
        os << ",\"cancel\":{" << (cancel.empty() ? "" : "\"press\":" + q(cancel) + ",") << "\"key\":\"Esc\"}";
    os << "}";
    return os.str();
}

// The window a press goes to when no id is given: the running modal dialog, else
// the front-most dialog on the desktop.
TGroup* defaultPressTarget() {
    if (TheTopView && !dynamic_cast<TMenuView*>(TheTopView)) return dynamic_cast<TGroup*>(TheTopView);
    TDeskTop* d = TProgram::deskTop;
    if (!d || !d->first()) return nullptr;
    TView* f = d->first();
    TView* v = f;
    do {
        if (dynamic_cast<TDialog*>(v) && (v->state & sfVisible)) return static_cast<TGroup*>(v);
        v = v->next;
    } while (v != f);
    return nullptr;
}

struct Found { TView* control = nullptr; int index = -1; std::string kind; };

Found findControl(TGroup* g, const std::string& label, const std::string& item) {
    auto labels = labelsOf(g);
    std::string key = matchKey(label);
    std::string ikey = matchKey(item);
    Found exact, prefix;
    int prefixCount = 0;
    auto consider = [&](TView* c, int idx, const std::string& text, const char* kind) {
        std::string k = matchKey(text);
        if (k.empty()) return;
        if (k == key && !exact.control) { exact.control = c; exact.index = idx; exact.kind = kind; }
        else if (k.rfind(key, 0) == 0) { if (!prefixCount++) { prefix.control = c; prefix.index = idx; prefix.kind = kind; } }
    };
    auto indexOf = [&](TView* c) -> int {
        if (ikey.empty()) return -1;
        if (auto* cl = dynamic_cast<TCluster*>(c)) {
            auto items = clusterItems(cl);
            for (size_t i = 0; i < items.size(); ++i) if (matchKey(items[i]) == ikey) return (int)i;
        }
        if (auto* lv = dynamic_cast<TListViewer*>(c)) {
            for (int i = 0; i < lv->range; ++i) if (matchKey(listItemText(lv, i)) == ikey) return i;
        }
        return -2;
    };
    for (TView* c : childrenInOrder(g)) {
        if (auto* b = dynamic_cast<TButton*>(c)) { consider(c, -1, plainLabel(b->title), "button"); continue; }
        if (labels.count(c)) consider(c, indexOf(c), labels[c], "labelled");
        if (auto* cl = dynamic_cast<TCluster*>(c)) {
            auto items = clusterItems(cl);
            for (size_t i = 0; i < items.size(); ++i) consider(c, (int)i, items[i], "item");
        } else if (auto* lv = dynamic_cast<TListViewer*>(c)) {
            int n = std::min<int>(lv->range, 500);
            for (int i = 0; i < n; ++i) consider(c, i, listItemText(lv, i), "item");
        }
    }
    if (exact.control) return exact;
    if (prefixCount == 1) return prefix;
    return Found{};
}

TMenuItem* findMenuItem(TMenu* menu, const std::vector<std::string>& path, size_t at) {
    for (TMenuItem* it = menu ? menu->items : nullptr; it; it = it->next) {
        if (!it->name) continue;
        if (matchKey(plainLabel(it->name)) != matchKey(path[at])) continue;
        if (at + 1 == path.size()) return it;
        if (it->command == 0 && it->subMenu) {
            if (TMenuItem* r = findMenuItem(it->subMenu, path, at + 1)) return r;
        }
    }
    return nullptr;
}

TMenuItem* searchMenu(TMenu* menu, const std::string& name, int& count, int depth) {
    TMenuItem* found = nullptr;
    for (TMenuItem* it = menu ? menu->items : nullptr; it; it = it->next) {
        if (!it->name) continue;
        if (it->command != 0 && matchKey(plainLabel(it->name)) == matchKey(name)) { if (!found) found = it; ++count; }
        if (it->command == 0 && it->subMenu && depth < 6) {
            TMenuItem* r = searchMenu(it->subMenu, name, count, depth + 1);
            if (r && !found) found = r;
        }
    }
    return found;
}

}  // namespace

std::string cmdFront(const KV&) {
    TDeskTop* d = TProgram::deskTop;
    TView* front = TheTopView ? TheTopView : (d ? d->current : nullptr);
    std::ostringstream os;
    os << "{\"modal\":" << (TheTopView ? "true" : "false")
       << ",\"menu_open\":" << (dynamic_cast<TMenuView*>(TheTopView) ? "true" : "false")
       << ",\"focused\":" << q(d && d->current ? nameOf(d->current) : "")
       << ",\"front\":" << describe(front) << ",\"dialogs\":[";
    bool first = true;
    if (d && d->first()) {
        TView* f = d->first();
        TView* v = f;
        do {
            if (dynamic_cast<TDialog*>(v) && v != front) {
                os << (first ? "" : ",") << describe(v);
                first = false;
            }
            v = v->next;
        } while (v != f);
    }
    os << "]}";
    return os.str();
}

std::string cmdPress(const KV& kv) {
    std::string label = str(kv, "label");
    if (label.empty()) return "err missing label";
    std::string err;
    TView* t = resolveTarget(kv, err);
    if (!err.empty()) return err;
    TGroup* g = t ? dynamic_cast<TGroup*>(t) : defaultPressTarget();
    if (!g) return "err no dialog in front; give id=";
    Found f = findControl(g, label, str(kv, "item"));
    if (!f.control) return "err no control labelled '" + label + "' in " + nameOf(g);
    if (f.index == -2) return "err no item '" + str(kv, "item") + "'";
    if (personActingIn(g)) return sharingError(g);
    Item it;
    it.kind = Kind::Press;
    it.target = g;
    it.targetId = nameOf(g);
    it.control = f.control;
    it.index = f.index;
    if (has(kv, "value")) { it.hasValue = true; it.value = str(kv, "value"); }
    it.what = "press " + label + (f.index >= 0 ? " #" + std::to_string(f.index) : "");
    std::vector<Item> items{it};
    return enqueueAll(items);
}

std::string cmdMenu(const KV& kv) {
    TMenuView* bar = TProgram::menuBar;
    TMenu* menu = bar ? bar->*PeekMenuView::menu_() : nullptr;
    if (!has(kv, "choose")) {
        std::ostringstream os;
        os << "{\"open\":" << (dynamic_cast<TMenuView*>(TheTopView) ? "true" : "false")
           << ",\"menus\":" << menuItemsJson(menu, 0) << "}";
        return os.str();
    }
    std::string path = str(kv, "choose");
    std::vector<std::string> parts;
    {
        std::string cur;
        for (char c : path) {
            if (c == '>' || c == '/') { parts.push_back(cur); cur.clear(); }
            else cur += c;
        }
        parts.push_back(cur);
    }
    TMenuItem* item = nullptr;
    if (parts.size() > 1) item = findMenuItem(menu, parts, 0);
    else {
        int count = 0;
        item = searchMenu(menu, parts[0], count, 0);
        if (count > 1) return "err menu item '" + path + "' is ambiguous; give the path, e.g. File>Open";
    }
    if (!item || item->command == 0) return "err no menu item '" + path + "'";
    if (item->disabled || !TView::commandEnabled(item->command)) return "err menu item disabled: " + path;
    std::string err;
    TView* t = resolveTarget(kv, err);
    if (!err.empty()) return err;
    if (t && personActingIn(t)) return sharingError(t);
    Item it;
    it.kind = Kind::Command;
    it.ev = TEvent{};
    it.ev.what = evCommand;
    it.ev.message.command = item->command;
    it.ev.message.infoPtr = nullptr;
    it.target = t;
    it.targetId = t ? nameOf(t) : "app";
    it.what = "menu " + path + " (command " + std::to_string(item->command) + ")";
    std::vector<Item> items{it};
    return enqueueAll(items);
}

}  // namespace resident
