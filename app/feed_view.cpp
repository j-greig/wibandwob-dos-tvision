// The Feed window. See feed_view.h.
#define Uses_TWindow
#define Uses_TScroller
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TEvent
#include <tvision/tv.h>

#include "feed_view.h"
#include "resident_senses.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <set>

namespace {

std::set<TFeedView*>& feeds() {
    static std::set<TFeedView*> s;
    return s;
}

std::string tagOf(const resident::ActRecord& a) {
    if (a.source == "console") return "PER";
    if (a.source == "app") return "SYS";
    if (a.actor == "wib") return "WIB";
    if (a.actor == "wob") return "WOB";
    if (a.actor == "scramble") return "SCR";
    if (a.actor == "api") return "API";
    if (a.source == "guest") return "GST";
    if (a.source == "resident") return "RES";
    return "VIS";
}

// BIOS colours on black: one per tag, so each kind of actor is told apart at a glance.
TColorAttr colourOf(const std::string& tag) {
    int fg = tag == "PER" ? 15 : tag == "WIB" ? 14 : tag == "WOB" ? 11 : tag == "SCR" ? 13 : tag == "VIS" ? 10
           : tag == "GST" ? 2 : tag == "API" ? 6 : tag == "RES" ? 9 : 8;
    return TColorAttr(TColorBIOS(fg), TColorBIOS(0));
}

std::string clock(long long ms) {
    using namespace std::chrono;
    long long ago = resident::actsClockMs() - ms;
    std::time_t t = system_clock::to_time_t(system_clock::now() - milliseconds(ago));
    char b[16];
    std::strftime(b, sizeof b, "%H:%M:%S", std::localtime(&t));
    return b;
}

const char* kFilterKeys = "apwosvy";   // all, person, wib, wob, scramble, visitors, system
std::string filterForKey(char k) {
    switch (k) {
        case 'p': return "PER";
        case 'w': return "WIB";
        case 'o': return "WOB";
        case 's': return "SCR";
        case 'v': return "VIS";
        case 'y': return "SYS";
        default: return "";
    }
}

}  // namespace

TFeedView::TFeedView(const TRect& r, TScrollBar* v) : TScroller(r, nullptr, v) {
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable;
    feeds().insert(this);
    refresh(true);
}

TFeedView::~TFeedView() { feeds().erase(this); }

void TFeedView::tickAll() {
    for (TFeedView* f : feeds()) f->refresh();
}

void TFeedView::setFilter(const std::string& tag) {
    filter_ = tag;
    follow_ = true;
    refresh(true);
}

void TFeedView::refresh(bool force) {
    unsigned long long rev = resident::actsRevision();
    if (!force && rev == seenRevision_) return;
    seenRevision_ = rev;
    std::vector<resident::ActRecord> acts;
    resident::lastActs(1000, acts);
    lines_.clear();
    tags_.clear();
    refused_.clear();
    // A resident's keys arrive one act per key: a run of printable keys by one actor in one window,
    // all delivered, reads as one line (typed "..."), as the person's typing already does.
    std::vector<resident::ActRecord> runs;
    for (const auto& a : acts) {
        bool oneKey = a.what.rfind("key ", 0) == 0 && (a.what.size() == 5 || a.what == "key Space" || a.what == "key Enter") &&
                      (a.result.empty() || a.result.rfind("delivered", 0) == 0);
        std::string ch = !oneKey ? "" : a.what == "key Space" ? " " : a.what == "key Enter" ? "/" : a.what.substr(4);
        if (oneKey && !runs.empty()) {
            auto& last = runs.back();
            if (last.actor == a.actor && last.source == a.source && last.window == a.window &&
                last.what.rfind("typed \"", 0) == 0 && last.what.back() == '"') {
                last.what.insert(last.what.size() - 1, ch);
                last.ms = a.ms;
                continue;
            }
        }
        runs.push_back(a);
        if (oneKey) runs.back().what = "typed \"" + ch + "\"";
    }
    // desk-mosh moves many shard windows per step (owner and actor "mosh"): a run of its acts is one line.
    std::vector<resident::ActRecord> folded;
    int moshRun = 0;
    for (const auto& a : runs) {
        if (a.actor == "mosh" && !folded.empty() && folded.back().actor == "mosh") {
            ++moshRun;
            folded.back().what = "mosh step: " + std::to_string(moshRun) + " acts (last: " + a.what + ")";
            folded.back().ms = a.ms;
            if (folded.back().window != a.window) folded.back().window = "";
            continue;
        }
        moshRun = 1;
        folded.push_back(a);
    }
    for (const auto& a : folded) {
        std::string tag = tagOf(a);
        if (!filter_.empty() && tag != filter_) continue;
        std::string who = a.actor;
        std::string own = tag == "PER" ? "person" : tag == "WIB" ? "wib" : tag == "WOB" ? "wob" : tag == "SCR" ? "scramble"
                        : tag == "SYS" ? "app" : "";
        std::string line = clock(a.ms) + " " + tag + " ";
        if (who != own) line += who + ": ";
        line += a.what;
        if (!a.window.empty()) line += "  [" + a.window + "]";
        bool refused = a.result.rfind("refused", 0) == 0 || a.result.rfind("err", 0) == 0;
        if (!a.result.empty() && a.result != "ok" && a.result != "delivered") line += "  " + a.result;
        lines_.push_back(line);
        tags_.push_back(tag);
        refused_.push_back(refused);
    }
    int width = 0;
    for (auto& l : lines_) width = std::max(width, (int)l.size());
    setLimit(width + 1, (int)lines_.size());
    if (follow_) scrollTo(0, std::max(0, (int)lines_.size() - size.y));
    drawView();
}

void TFeedView::draw() {
    TDrawBuffer b;
    for (int y = 0; y < size.y; ++y) {
        int i = delta.y + y;
        b.moveChar(0, ' ', TColorAttr(TColorBIOS(7), TColorBIOS(0)), size.x);
        if (i >= 0 && i < (int)lines_.size()) {
            const std::string& l = lines_[i];
            std::string shown = (int)l.size() > delta.x ? l.substr(delta.x) : "";
            TColorAttr c = refused_[i] ? TColorAttr(TColorBIOS(12), TColorBIOS(0)) : colourOf(tags_[i]);
            b.moveStr(0, shown.c_str(), c);
        }
        writeLine(0, y, size.x, 1, b);
    }
}

void TFeedView::handleEvent(TEvent& ev) {
    if (ev.what == evKeyDown) {
        char ch = ev.keyDown.charScan.charCode;
        if (ch && std::string(kFilterKeys).find(ch) != std::string::npos) {
            setFilter(filterForKey(ch));
            clearEvent(ev);
            return;
        }
        if (ev.keyDown.keyCode == kbEnd || ev.keyDown.keyCode == kbCtrlEnd) {
            follow_ = true;
            refresh(true);
            clearEvent(ev);
            return;
        }
    }
    int before = delta.y;
    TScroller::handleEvent(ev);
    if (delta.y != before) follow_ = delta.y >= (int)lines_.size() - size.y;   // scrolled back: stop following
}

TFeedWindow::TFeedWindow(const TRect& r) : TWindowInit(&TWindow::initFrame), TWindow(r, "Feed", wnNoNumber) {
    options |= ofTileable;
    TScrollBar* v = standardScrollBar(sbVertical | sbHandleKeyboard);
    TRect in = getExtent();
    in.grow(-1, -1);
    view_ = new TFeedView(in, v);
    insert(view_);
}

std::string TFeedWindow::describeState() const {
    if (!view_) return "feed of the act log";
    return "feed of the act log: " + std::to_string(view_->lines().size()) + " lines" +
           (view_->filter().empty() ? std::string(", every actor") : ", only " + view_->filter()) +
           (view_->following() ? ", following the newest" : ", scrolled back");
}

bool TFeedWindow::describeText(std::vector<std::string>& out) const {
    if (!view_) return false;
    out = view_->lines();
    return true;
}

TWindow* createFeedWindow(const TRect& r, const std::string& by) {
    auto* w = new TFeedWindow(r);
    if (!by.empty() && w->view()) {
        std::string t = by == "person" ? "PER" : by == "wib" ? "WIB" : by == "wob" ? "WOB" : by == "scramble" ? "SCR"
                      : by == "visitor" ? "VIS" : by == "system" || by == "app" ? "SYS" : by;
        w->view()->setFilter(t);
    }
    return w;
}
