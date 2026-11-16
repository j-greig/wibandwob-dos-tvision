#include "score_view.h"

#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_TProgram
#include <tvision/tv.h>

#include "ears.h"
#include "resident_senses.h"
#include "sound_host.h"
#include "stream.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <cstdio>
#include <sstream>

using namespace sound;

TScoreView::TScoreView(const TRect& bounds, const Score& score, const std::string& origin)
    : TView(bounds), score_(score), origin_(origin) {
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable | ofFirstClick;
    eventMask |= evBroadcast | evKeyDown;
}

TScoreView::~TScoreView() {
    if (timer_) killTimer(timer_);
}

std::string TScoreView::windowId() const {
    TWindow* w = dynamic_cast<TWindow*>(owner);
    return (w && host().idOf) ? host().idOf(w) : "";
}

// The score acts as "score" (source app): its section changes and its do lines.
static const resident::Who kScoreWho{"score", resident::Source::App};

static void logAct(const std::string& what, const std::string& window, const std::string& result) {
    resident::addAct(kScoreWho, what, window, result);
}

void TScoreView::start() {
    if (timer_) { killTimer(timer_); timer_ = nullptr; }
    running_ = true;
    done_ = false;
    startMs_ = clockMs();
    section_ = -1;
    startedBy_ = currentActor();
    actResults_.clear();
    timer_ = setTimer(20, 20);
    tick();
}

void TScoreView::stop(const char* why) {
    if (!running_) return;
    running_ = false;
    stopMs_ = clockMs();
    if (timer_) { killTimer(timer_); timer_ = nullptr; }
    logAct(std::string("score \"") + score_.title + "\" " + why, windowId(), "");
    drawView();
}

void TScoreView::enter(int i) {
    section_ = i;
    const ScoreSection& s = score_.sections[i];
    logAct("score \"" + score_.title + "\" section " + std::to_string(i + 1) + " of " +
               std::to_string(score_.sections.size()) + ": " + s.who + ": " + s.text,
           windowId(), "");
    // The machine's own acts, from the fixed list parseScore allows.
    resident::WhoScope as(kScoreWho);
    for (const auto& a : s.acts) {
        std::string r;
        if (a.target == "tr808") {
            KV kv{{"action", a.verb == "preset" ? "load_preset" : a.verb}};
            if (a.verb == "preset") kv["preset"] = a.arg;
            if (a.verb == "tempo") kv["bpm"] = a.arg;
            // Start the drums on the section's bar line, so bars and steps share one origin on one clock.
            if (a.verb == "play") kv["at_ms"] = std::to_string(startMs_ + sectionStartMs(score_, i));
            r = host().tr808 ? host().tr808(kv) : "err no tr808 route";
        } else {
            r = command("synth", {{"action", a.verb}, {"value", a.arg}});
        }
        const std::string result = r.rfind("err", 0) == 0 ? r : "ok";
        actResults_.push_back(a.text() + ": " + result);
        logAct("score do " + a.text(), windowId(), result);
    }
}

void TScoreView::tick() {
    if (!running_) return;
    const ScorePos p = positionAt(score_, clockMs() - startMs_);
    while (section_ < p.section && section_ + 1 < (int)score_.sections.size()) enter(section_ + 1);
    if (p.done) {
        done_ = true;
        stop("finished");
        return;
    }
    drawView();
}

TScoreView::Heard TScoreView::heardIn(int i) const {
    Heard h;
    if (section_ < 0 || i > section_) return h;
    const double a = startMs_ + sectionStartMs(score_, i);
    const double b = std::min(a + sectionMs(score_, i), running_ ? clockMs() : stopMs_);
    if (b <= a) return h;
    for (const auto& e : ears().since(b, b - a)) {
        if (e.startMs < a || e.startMs >= b) continue;
        if (e.source == "synth") {
            h.notes.push_back(noteName(e.midi));
            h.by[e.who]++;
        } else {
            ++h.hits;
        }
    }
    return h;
}

void TScoreView::handleEvent(TEvent& ev) {
    TView::handleEvent(ev);
    if (ev.what == evBroadcast && ev.message.command == cmTimerExpired) {
        if (timer_ && ev.message.infoPtr == timer_) {
            tick();
            clearEvent(ev);
        }
        return;
    }
    if (ev.what != evKeyDown) return;
    if (ev.keyDown.controlKeyState & kbAltShift) return;
    if (ev.keyDown.keyCode == kbEnter || ev.keyDown.charScan.charCode == ' ') {
        if (running_) stop("stopped"); else start();
        clearEvent(ev);
    }
}

// ── Draw ──────────────────────────────────────────────────────────────────

static std::string byText(const std::map<std::string, int>& by) {
    std::string s;
    for (const auto& b : by) s += (s.empty() ? "" : ", ") + b.first + " " + std::to_string(b.second);
    return s;
}

void TScoreView::draw() {
    TColorAttr base, dim, bright, now;
    TColorAttr themed;
    if (ww::contentAttr(this, 15, themed)) {
        const TColor bg = themed.getBackground();
        base = themed;
        dim = TColorAttr(ww::egaRGB(7), bg);
        bright = TColorAttr(ww::egaRGB(15), bg);
        now = TColorAttr(ww::egaRGB(0), ww::egaRGB(11));
    } else {
        const TColorRGB bk(0, 0, 0);
        base = TColorAttr(TColorRGB(0xAA, 0xAA, 0xAA), bk);
        dim = TColorAttr(TColorRGB(0x77, 0x77, 0x77), bk);
        bright = TColorAttr(TColorRGB(0xFF, 0xFF, 0xFF), bk);
        now = TColorAttr(bk, TColorRGB(0xFF, 0xFF, 0xFF));
    }
    const int W = size.x, H = size.y;
    const ScorePos p = positionAt(score_, (running_ ? clockMs() : stopMs_) - startMs_);
    const int listRows = std::max(0, H - 6);
    const int first = std::max(0, std::min(section_ - listRows / 2, (int)score_.sections.size() - listRows));
    for (int y = 0; y < H; ++y) {
        TDrawBuffer b;
        b.moveChar(0, ' ', base, (ushort)W);
        if (y == 0) {
            std::ostringstream o;
            o << "EVENT SCORE  " << score_.title << "  " << score_.tempo << " bpm  "
              << (running_ ? "\u25b6 running" : done_ ? "finished" : "\u25a0 stopped");
            b.moveStr(1, o.str(), bright);
        } else if (y >= 2 && y < 2 + listRows) {
            const int i = first + (y - 2);
            if (i >= 0 && i < (int)score_.sections.size()) {
                const ScoreSection& s = score_.sections[i];
                const bool cur = running_ && i == section_;
                char head[48];
                std::snprintf(head, sizeof(head), "%s%2d  %2d bar%s  %-7s ", cur ? "\u25b6" : (i < section_ || done_) ? "\u2713" : " ",
                              i + 1, s.bars, s.bars == 1 ? " " : "s", s.who.c_str());
                std::string line = std::string(head) + s.text;
                if (i <= section_) {
                    const Heard h = heardIn(i);
                    if (s.kind == "rest") line += h.notes.empty() ? "  [kept]" : "  [broken: " + byText(h.by) + "]";
                    else if (!h.notes.empty()) line += "  [" + std::to_string(h.notes.size()) + " notes: " + byText(h.by) + "]";
                }
                b.moveStr(1, line, cur ? now : (i < section_ ? dim : base), (ushort)std::max(0, W - 2));
            }
        } else if (y == H - 3 && running_ && section_ >= 0) {
            const ScoreSection& s = score_.sections[section_];
            b.moveStr(1, "NOW  " + s.who + ": " + s.text, bright, (ushort)std::max(0, W - 2));
        } else if (y == H - 2 && running_ && section_ >= 0) {
            std::string beats;
            for (int k = 0; k < score_.beatsPerBar; ++k) beats += k == p.beat ? "\u25cf " : "\u25cb ";
            b.moveStr(1, "bar " + std::to_string(p.bar + 1) + " of " + std::to_string(score_.sections[section_].bars) + "  " + beats, base);
        } else if (y == H - 1) {
            b.moveStr(1, "Enter start or stop", dim);
        }
        writeLine(0, (short)y, (short)W, 1, b);
    }
}

// ── API ───────────────────────────────────────────────────────────────────

std::string TScoreView::stateJson() const {
    const double t = (running_ ? clockMs() : stopMs_) - startMs_;
    const ScorePos p = positionAt(score_, t);
    const int n = (int)score_.sections.size();
    std::ostringstream o;
    o << "{\"ok\":true,\"standing\":\"data\",\"title\":\"" << jsonEscape(score_.title) << "\",\"from\":\""
      << jsonEscape(origin_) << "\",\"tempo\":" << score_.tempo << ",\"beats\":" << score_.beatsPerBar
      << ",\"bar_ms\":" << (long)barMs(score_) << ",\"running\":" << (running_ ? "true" : "false")
      << ",\"done\":" << (done_ ? "true" : "false") << ",\"started_by\":\"" << jsonEscape(startedBy_) << "\""
      << ",\"started_ms\":" << std::to_string(startMs_) << ",\"lead_ms\":" << std::to_string(leadMs());
    o << ",\"parts\":[";
    for (size_t i = 0; i < score_.parts.size(); ++i)
        o << (i ? "," : "") << "{\"name\":\"" << jsonEscape(score_.parts[i].first) << "\",\"is\":\""
          << jsonEscape(score_.parts[i].second) << "\"}";
    o << "]";
    if (running_ && section_ >= 0 && section_ < n) {
        const ScoreSection& s = score_.sections[section_];
        o << ",\"section\":" << section_ + 1 << ",\"of\":" << n << ",\"bar\":" << p.bar + 1 << ",\"bars\":" << s.bars
          << ",\"beat\":" << p.beat + 1 << ",\"ms_left\":" << (long)p.leftMs
          << ",\"now\":{\"who\":\"" << jsonEscape(s.who) << "\",\"text\":\"" << jsonEscape(s.text) << "\",\"kind\":\"" << s.kind << "\"}";
        if (section_ + 1 < n) {
            const ScoreSection& nx = score_.sections[section_ + 1];
            o << ",\"next\":{\"who\":\"" << jsonEscape(nx.who) << "\",\"text\":\"" << jsonEscape(nx.text)
              << "\",\"in_ms\":" << (long)p.leftMs << "}";
        }
        if (s.kind == "answer" && section_ > 0) {
            const Heard h = heardIn(section_ - 1);
            o << ",\"answering\":{\"section\":" << section_ << ",\"notes\":[";
            for (size_t i = 0; i < h.notes.size(); ++i) o << (i ? "," : "") << "\"" << h.notes[i] << "\"";
            o << "],\"by\":\"" << jsonEscape(byText(h.by)) << "\"}";
        }
    }
    o << ",\"sections\":[";
    for (int i = 0; i < n; ++i) {
        const ScoreSection& s = score_.sections[i];
        const char* stateWord = (done_ || (section_ >= 0 && i < section_)) ? "done" : (running_ && i == section_) ? "now" : "ahead";
        o << (i ? "," : "") << "{\"n\":" << i + 1 << ",\"bars\":" << s.bars << ",\"who\":\"" << jsonEscape(s.who)
          << "\",\"text\":\"" << jsonEscape(s.text) << "\",\"kind\":\"" << s.kind << "\",\"state\":\"" << stateWord << "\"";
        if (!s.acts.empty()) {
            o << ",\"do\":[";
            for (size_t k = 0; k < s.acts.size(); ++k) o << (k ? "," : "") << "\"" << jsonEscape(s.acts[k].text()) << "\"";
            o << "]";
        }
        if (section_ >= 0 && i <= section_) {
            const Heard h = heardIn(i);
            o << ",\"heard\":{\"notes\":[";
            for (size_t k = 0; k < h.notes.size(); ++k) o << (k ? "," : "") << "\"" << h.notes[k] << "\"";
            o << "],\"by\":{";
            bool f = true;
            for (const auto& b : h.by) { o << (f ? "" : ",") << "\"" << jsonEscape(b.first) << "\":" << b.second; f = false; }
            o << "},\"hits\":" << h.hits << "}";
            if (s.kind == "rest") o << ",\"kept\":" << (h.notes.empty() ? "true" : "false");
        }
        o << "}";
    }
    o << "],\"acts\":[";
    for (size_t i = 0; i < actResults_.size(); ++i) o << (i ? "," : "") << "\"" << jsonEscape(actResults_[i]) << "\"";
    o << "]}";
    return o.str();
}

bool TScoreView::describeText(std::vector<std::string>& out) const {
    for (size_t i = 0; i < score_.sections.size(); ++i) {
        const ScoreSection& s = score_.sections[i];
        out.push_back(std::to_string(i + 1) + ". " + std::to_string(s.bars) + " bar" + (s.bars == 1 ? "" : "s") + ", " + s.who + ": " + s.text);
    }
    return true;
}

std::string TScoreView::sentence() const {
    std::ostringstream o;
    o << "event score \"" << score_.title << "\"";
    const int n = (int)score_.sections.size();
    if (running_ && section_ >= 0 && section_ < n) {
        const ScorePos p = positionAt(score_, clockMs() - startMs_);
        const ScoreSection& s = score_.sections[section_];
        o << ", running: section " << section_ + 1 << " of " << n << ", bar " << p.bar + 1 << " of " << s.bars
          << "; now " << s.who << ": " << s.text;
    } else if (done_) {
        o << ", finished: " << n << " sections played";
    } else {
        int bars = 0;
        for (const auto& s : score_.sections) bars += s.bars;
        o << ", stopped: " << n << " sections, " << bars << " bars at " << score_.tempo << " bpm";
    }
    return o.str();
}

std::string TScoreView::apiAction(const std::string& action, const std::map<std::string, std::string>&) {
    if (action == "start" || action == "play") start();
    else if (action == "stop") stop("stopped");
    else if (!(action.empty() || action == "state" || action == "open")) return "err unknown action";
    drawView();
    return stateJson();
}

// ── Window ────────────────────────────────────────────────────────────────

TScoreWindow::TScoreWindow(const TRect& bounds, const Score& score, const std::string& origin)
    : TWindowInit(&TScoreWindow::initFrame), TWindow(bounds, "Event Score", wnNoNumber) {
    options |= ofTileable;
    TRect c = getExtent();
    c.grow(-1, -1);
    view_ = new TScoreView(c, score, origin);
    insert(view_);
}
