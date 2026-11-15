// The cast player: a libvterm screen (tvterm's VTermEmulator) fed from a cast's events by
// the clock, drawn as ordinary cells, with a control row under it.
#define Uses_TWindow
#define Uses_TView
#define Uses_TRect
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TDrawBuffer
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TScreenCell
#define Uses_TFrame
#include <tvision/tv.h>

#include "cast_player.h"
#include "cast_record.h"

#include <tvterm/termemu.h>
#include <tvterm/vtermemu.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {

struct NullWriter : tvterm::Writer {
    void write(TSpan<const char>) noexcept override {}
};

std::string fmt1(double v) {
    char b[32];
    snprintf(b, sizeof b, "%.1f", v);
    return b;
}

std::string fmtSpeed(double v) {
    char b[32];
    snprintf(b, sizeof b, v < 1 ? "x%.2g" : "x%g", v);
    return b;
}

std::string jsonEsc(const std::string& s) { return ww::castEscape(s); }

}  // namespace

class TCastView : public TView {
public:
    ww::Cast cast;
    double pos = 0, speed = 1;
    bool playing = true, loop = false;

    TCastView(const TRect& r, ww::Cast c) : TView(r), cast(std::move(c)) {
        options |= ofSelectable | ofFirstClick;
        eventMask |= evBroadcast | evMouseDown | evKeyDown;
        growMode = gfGrowHiX | gfGrowHiY;
        reset();
        apply(0);
        last_ = std::chrono::steady_clock::now();
    }
    ~TCastView() { if (timer_) killTimer(timer_); }

    // The play clock runs while the view is on screen (the same pattern as the animated views).
    void setState(ushort aState, Boolean enable) override {
        TView::setState(aState, enable);
        if (aState & sfExposed) {
            if (enable && !timer_) { last_ = std::chrono::steady_clock::now(); timer_ = setTimer(33, 33); }
            else if (!enable && timer_) { killTimer(timer_); timer_ = 0; }
        }
    }

    void reset() {
        emu_.reset(new tvterm::VTermEmulator({cast.width, cast.height}, writer_));
        state_.reset(new tvterm::TerminalState);
        next_ = 0;
    }

    // Feeds every event up to time t (re-reading from the start when t is earlier).
    void apply(double t) {
        while (next_ < cast.events.size() && cast.events[next_].t <= t + 1e-9) {
            const auto& e = cast.events[next_++];
            tvterm::TerminalEvent ev;
            ev.type = tvterm::TerminalEventType::ClientDataRead;
            ev.clientDataRead = {e.data.data(), e.data.size()};
            emu_->handleEvent(ev);
        }
        emu_->updateState(*state_);
        pos = t;
        drawView();
    }

    void seek(double t) {
        t = std::max(0.0, std::min(t, cast.duration()));
        if (t < pos) { reset(); pos = 0; }
        apply(t);
    }

    void setPlaying(bool on) {
        if (on && pos >= cast.duration()) seek(0);
        playing = on;
        last_ = std::chrono::steady_clock::now();
        drawView();
        if (auto* w = dynamic_cast<TWindow*>(owner)) if (w->frame) w->frame->drawView();
    }

    // The screen as text, trailing blanks dropped (the same shape as terminal_read).
    std::vector<std::string> screenText() const {
        std::vector<std::string> out;
        auto& s = state_->surface;
        for (int y = 0; y < s.size.y; ++y) {
            std::string row;
            for (int x = 0; x < s.size.x; ++x) {
                const TScreenCell& c = s.at(y, x);
                if (c.character.isWideCharTrail()) continue;
                TStringView sv = c.character.getText();
                if (sv.empty() || sv[0] == '\0') row += ' ';
                else row.append(sv.data(), sv.size());
            }
            size_t e = row.find_last_not_of(' ');
            row.erase(e == std::string::npos ? 0 : e + 1);
            out.push_back(row);
        }
        return out;
    }

    void draw() override {
        auto& s = state_->surface;
        TColorAttr blank = TColorAttr(TColorBIOS(0x7), TColorBIOS(0x0));
        std::vector<TScreenCell> row(std::max<int>(size.x, 1));
        for (int y = 0; y < size.y - 1; ++y) {
            for (int x = 0; x < size.x; ++x) {
                TScreenCell c;
                if (y < s.size.y && x < s.size.x) {
                    c = s.at(y, x);
                    TStringView sv = c.character.getText();
                    if (!c.character.isWideCharTrail() && (sv.empty() || sv[0] == '\0')) c.character = TScreenCharacter(' ');
                } else {
                    c = TScreenCell(TScreenCharacter(' '), blank);
                }
                row[x] = c;
            }
            writeLine(0, y, size.x, 1, row.data());
        }
        drawControls();
    }

    // The control row: [◄◄] [►]/[‖] [►►] time, speed [-][+], loop [L].
    void drawControls() {
        TDrawBuffer b;
        TColorAttr col = TColorAttr(TColorBIOS(0x7), TColorBIOS(0x0));
        TColorAttr hot = TColorAttr(TColorBIOS(0x0), TColorBIOS(0x7));
        b.moveChar(0, ' ', col, size.x);
        controls_.clear();
        int x = 0;
        auto put = [&](const std::string& s, const char* action, bool button) {
            int w = b.moveStr(x, s, button ? hot : col);
            if (action) controls_.push_back({x, x + w, action});
            x += w;
        };
        put("[◄◄]", "back", true);
        put(playing ? "[‖]" : "[►]", "toggle", true);
        put("[►►]", "forward", true);
        put(" " + fmt1(pos) + "/" + fmt1(cast.duration()) + "s ", nullptr, false);
        put("[-]", "slower", true);
        put(fmtSpeed(speed), nullptr, false);
        put("[+]", "faster", true); put(" ", nullptr, false);
        put(loop ? "[loop]" : "[once]", "loop", true);
        writeLine(0, size.y - 1, size.x, 1, b);
    }

    void doAction(const std::string& a) {
        if (a == "toggle") setPlaying(!playing);
        else if (a == "back") seek(pos - 5);
        else if (a == "forward") seek(pos + 5);
        else if (a == "slower") speed = std::max(0.25, speed / 2);
        else if (a == "faster") speed = std::min(8.0, speed * 2);
        else if (a == "loop") loop = !loop;
        drawView();
    }

    void handleEvent(TEvent& ev) override {
        TView::handleEvent(ev);
        if (ev.what == evBroadcast && ev.message.command == cmTimerExpired && ev.message.infoPtr == timer_) {
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - last_).count();
            last_ = now;
            if (playing) {
                double t = pos + dt * speed;
                if (t >= cast.duration()) {
                    if (loop && cast.duration() > 0) { seek(cast.duration()); seek(0); }
                    else { apply(cast.duration()); playing = false; drawView(); }
                } else {
                    apply(t);
                }
            }
            return;   // a broadcast: other views get it too
        }
        if (ev.what == evMouseDown) {
            TPoint p = makeLocal(ev.mouse.where);
            if (p.y == size.y - 1) {
                for (auto& c : controls_)
                    if (p.x >= c.x0 && p.x < c.x1) { doAction(c.action); break; }
                clearEvent(ev);
            }
            return;
        }
        if (ev.what == evKeyDown) {
            ushort k = ev.keyDown.keyCode;
            char ch = ev.keyDown.charScan.charCode;
            bool used = true;
            if (ch == ' ') doAction("toggle");
            else if (k == kbLeft) seek(pos - 1);
            else if (k == kbRight) seek(pos + 1);
            else if (k == kbHome) seek(0);
            else if (k == kbEnd) seek(cast.duration());
            else if (ch == '-') doAction("slower");
            else if (ch == '+' || ch == '=') doAction("faster");
            else if (ch == 'l' || ch == 'L') doAction("loop");
            else if (ch >= '0' && ch <= '9') seek(cast.duration() * (ch - '0') / 10.0);
            else used = false;
            if (used) clearEvent(ev);
        }
    }

private:
    struct Control { int x0, x1; const char* action; };
    NullWriter writer_;
    std::unique_ptr<tvterm::VTermEmulator> emu_;
    std::unique_ptr<tvterm::TerminalState> state_;
    size_t next_ = 0;
    std::vector<Control> controls_;
    std::chrono::steady_clock::time_point last_;
    TTimerId timer_ = 0;
};

TCastPlayerWindow::TCastPlayerWindow(const TRect& r, const std::string& file, TCastView* view)
    : TWindowInit(&TWindow::initFrame), TWindow(r, nullptr, wnNoNumber), view_(view), file_(file) {
    std::string t = "cast: " + file.substr(file.find_last_of('/') == std::string::npos ? 0 : file.find_last_of('/') + 1);
    title = newStr(t.c_str());
    options |= ofTileable;
    eventMask |= evBroadcast;   // the view's play clock is a timer broadcast
    insert(view_);
}

std::string TCastPlayerWindow::stateJson() const {
    const auto* v = view_;
    return "{\"file\":\"" + jsonEsc(file_) + "\",\"width\":" + std::to_string(v->cast.width) +
           ",\"height\":" + std::to_string(v->cast.height) + ",\"events\":" + std::to_string(v->cast.events.size()) +
           ",\"duration\":" + fmt1(v->cast.duration()) + ",\"pos\":" + fmt1(v->pos) +
           ",\"playing\":" + (v->playing ? "true" : "false") + ",\"speed\":" + fmt1(v->speed) +
           ",\"loop\":" + (v->loop ? "true" : "false") + "}";
}

std::string TCastPlayerWindow::control(const std::map<std::string, std::string>& kv) {
    auto get = [&](const char* k) { auto it = kv.find(k); return it == kv.end() ? std::string() : it->second; };
    std::string a = get("action");
    if (!get("speed").empty()) {
        double s = atof(get("speed").c_str());
        if (s < 0.25 || s > 8) return "err speed is 0.25 to 8";
        view_->speed = s;
    }
    if (!get("loop").empty()) view_->loop = get("loop") == "1" || get("loop") == "true";
    if (!get("seek").empty()) view_->seek(atof(get("seek").c_str()));
    if (a == "play") view_->setPlaying(true);
    else if (a == "pause") view_->setPlaying(false);
    else if (a == "toggle") view_->setPlaying(!view_->playing);
    else if (!a.empty()) return "err action is play, pause or toggle";
    view_->drawView();
    return stateJson();
}

std::string TCastPlayerWindow::describeState() const {
    const auto* v = view_;
    return "cast player for " + file_ + ", " + std::to_string(v->cast.width) + "x" + std::to_string(v->cast.height) +
           ", at " + fmt1(v->pos) + " of " + fmt1(v->cast.duration()) + " s, " + (v->playing ? "playing" : "paused") +
           " at " + fmtSpeed(v->speed) + (v->loop ? ", looping" : "");
}

bool TCastPlayerWindow::describeText(std::vector<std::string>& out) const {
    out = view_->screenText();
    return true;
}

TCastPlayerWindow* createCastPlayer(const std::string& file, int x, int y, std::string& err) {
    ww::Cast cast;
    if (!ww::loadCast(file, cast, err)) return nullptr;
    int w = cast.width + 2, h = cast.height + 3;
    TRect desk = TProgram::deskTop ? TProgram::deskTop->getExtent() : TRect(0, 0, 80, 25);
    if (x < 0) x = std::max(0, (desk.b.x - w) / 2);
    if (y < 0) y = std::max(0, (desk.b.y - h) / 2);
    TRect r(x, y, x + w, y + h);
    TRect inner(1, 1, w - 1, h - 1);
    auto* view = new TCastView(inner, std::move(cast));
    return new TCastPlayerWindow(r, file, view);
}
