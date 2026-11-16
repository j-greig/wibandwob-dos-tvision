// Terminal transport: the registry commands, the app-level menu command and the act log.
// The process work (SIGSTOP/SIGCONT of the session's groups) is in tvterm_view.cpp.
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TEvent
#include <tvision/tv.h>

#include "terminal_transport.h"
#include "tvterm_view.h"
#include "cast_player.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace termtransport {
std::string idOf(TWibWobTerminalWindow* w);
std::string jsonEsc(const std::string& s);
namespace {

std::string str(const std::map<std::string, std::string>& kv, const char* k) {
    auto it = kv.find(k);
    return it == kv.end() ? std::string() : it->second;
}

}  // namespace

std::string jsonEsc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char) c; }
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += (char) c;
    }
    return o;
}

namespace {

const char* stateName(TWibWobTerminalWindow::RunState s) {
    switch (s) {
    case TWibWobTerminalWindow::RunState::Paused: return "paused";
    case TWibWobTerminalWindow::RunState::Exited: return "exited";
    default: return "running";
    }
}

}  // namespace

std::string idOf(TWibWobTerminalWindow* w) {
    const auto& h = resident::hooks();
    return h.idOf ? h.idOf(w) : std::string();
}

namespace {

std::vector<TWibWobTerminalWindow*> terminals() {
    std::vector<TWibWobTerminalWindow*> v;
    TDeskTop* d = TProgram::deskTop;
    if (!d || !d->first()) return v;
    TView* start = d->first();
    TView* p = start;
    do {
        if (auto* t = dynamic_cast<TWibWobTerminalWindow*>(p)) v.push_back(t);
        p = p->next;
    } while (p != start);
    return v;
}

std::string fmt1(double v) {
    char b[32];
    snprintf(b, sizeof b, "%.1f", v);
    return b;
}

}  // namespace

TWibWobTerminalWindow* target(const std::map<std::string, std::string>& kv, std::string& err) {
    std::string id = str(kv, "id");
    if (id.empty()) id = str(kv, "window_id");
    if (!id.empty()) {
        const auto& h = resident::hooks();
        TWindow* w = h.findWindow ? h.findWindow(id) : nullptr;
        auto* t = dynamic_cast<TWibWobTerminalWindow*>(w);
        if (!t) err = w ? "err window " + id + " is not a terminal" : "err window not found: " + id;
        return t;
    }
    TDeskTop* d = TProgram::deskTop;
    if (d && d->current)
        if (auto* t = dynamic_cast<TWibWobTerminalWindow*>(d->current)) return t;
    auto all = terminals();
    if (all.size() == 1) return all[0];
    err = all.empty() ? "err no terminal window" : "err several terminal windows: give id=";
    return nullptr;
}

std::string stateJson(TWibWobTerminalWindow* w) {
    auto st = w->runState();
    return "{\"id\":\"" + jsonEsc(idOf(w)) + "\",\"state\":\"" + stateName(st) + "\",\"program\":\"" +
           jsonEsc(st == TWibWobTerminalWindow::RunState::Exited ? std::string() : w->programName()) +
           "\",\"seconds_paused\":" + fmt1(w->secondsPaused()) + ",\"keyboard\":\"" + w->keyboard() + "\"}";
}

std::string act(TWibWobTerminalWindow* w, const std::string& action, const resident::Who& who) {
    if (!w) return "err no terminal window";
    std::string id = idOf(w);
    bool wasPaused = w->isPaused();
    bool wantPause = action == "pause" || (action == "toggle" && !wasPaused);
    if (action != "pause" && action != "resume" && action != "toggle") return "err unknown action " + action;
    std::string prog = w->programName();
    double heldFor = w->secondsPaused();
    std::string err;
    bool ok = wantPause ? w->pause(err) : w->resume(err);
    bool changed = ok && wasPaused != w->isPaused();
    std::string what = std::string(wantPause ? "paused" : "resumed") + " the terminal" +
                       (prog.empty() ? std::string() : " (" + prog + ")") +
                       (!wantPause && changed ? " after " + fmt1(heldFor) + " s" : std::string());
    if (!changed && ok) what += ", already so";
    resident::addAct(who, what, id, ok ? std::string() : "refused: " + err);
    if (!ok) return "err " + err;
    return "{\"ok\":true,\"id\":\"" + jsonEsc(id) + "\",\"state\":\"" + stateName(w->runState()) +
           "\",\"changed\":" + (changed ? "true" : "false") + ",\"program\":\"" + jsonEsc(prog) + "\"}";
}

bool handleCommand(unsigned short cmd) {
    if (cmd != cmTermToggle) return false;
    std::string err;
    std::map<std::string, std::string> none;
    if (TWibWobTerminalWindow* t = target(none, err)) act(t, "toggle", resident::currentWho());
    return true;
}

}  // namespace termtransport

std::string terminal_registry_command(const std::string& name, const std::map<std::string, std::string>& kv,
                                      bool* handled) {
    using namespace termtransport;
    *handled = true;
    if (name == "terminal_pause" || name == "terminal_resume" || name == "terminal_toggle") {
        std::string err;
        TWibWobTerminalWindow* t = target(kv, err);
        if (!t) return err;
        return act(t, name.substr(9), resident::currentWho());
    }
    if (name == "terminal_release") {
        std::string err;
        TWibWobTerminalWindow* t = target(kv, err);
        if (!t) return err;
        std::string id = idOf(t);
        bool released = t->releaseKeyboard();
        resident::addAct(resident::currentWho(), released ? "gave the keyboard back from the terminal"
                                                          : "release: the terminal did not hold the keyboard", id, "");
        return std::string("{\"ok\":true,\"id\":\"") + jsonEsc(id) + "\",\"released\":" +
               (released ? "true" : "false") + ",\"keyboard\":\"" + (released ? "focus" : t->keyboard()) + "\"}";
    }
    if (name == "terminal_record") {
        std::string err;
        TWibWobTerminalWindow* t = target(kv, err);
        if (!t) return err;
        std::string action = str(kv, "action");
        if (action.empty()) action = kv.count("stop") ? "stop" : kv.count("start") ? "start" : "";
        if (action == "start") {
            std::string file = str(kv, "file");
            if (file.empty()) return "err terminal_record action=start needs file=<path.cast>";
            if (!t->startRecording(file, err)) return "err " + err;
            return "{\"ok\":true,\"id\":\"" + jsonEsc(idOf(t)) + "\",\"recording\":\"" + jsonEsc(file) + "\"}";
        }
        if (action == "stop") {
            std::string file = t->recordingPath();
            double secs = t->recordingSeconds();
            long n = t->stopRecording(err);
            if (n < 0) return "err " + err;
            return "{\"ok\":true,\"id\":\"" + jsonEsc(idOf(t)) + "\",\"file\":\"" + jsonEsc(file) +
                   "\",\"events\":" + std::to_string(n) + ",\"seconds\":" + fmt1(secs) + "}";
        }
        if (action == "state" || action.empty()) {
            std::string file = t->recordingPath();
            return "{\"id\":\"" + jsonEsc(idOf(t)) + "\",\"recording\":" + (file.empty() ? "false" : "true") +
                   ",\"file\":\"" + jsonEsc(file) + "\",\"seconds\":" + fmt1(t->recordingSeconds()) + "}";
        }
        return "err action is start, stop or state";
    }
    if (name == "terminal_state") {
        std::string err;
        TWibWobTerminalWindow* t = target(kv, err);
        if (!t) return err;
        return stateJson(t);
    }
    if (name == "open_cast") {
        std::string file = str(kv, "file"), err;
        if (file.empty()) return "err open_cast needs file=<path.cast>";
        int x = kv.count("x") ? atoi(str(kv, "x").c_str()) : -1;
        int y = kv.count("y") ? atoi(str(kv, "y").c_str()) : -1;
        TCastPlayerWindow* w = createCastPlayer(file, x, y, err);
        if (!w) return "err " + err;
        if (kv.count("speed") || kv.count("loop") || str(kv, "paused") == "1") {
            std::map<std::string, std::string> c;
            if (kv.count("speed")) c["speed"] = str(kv, "speed");
            if (kv.count("loop")) c["loop"] = str(kv, "loop");
            if (str(kv, "paused") == "1") c["action"] = "pause";
            w->control(c);
        }
        TProgram::deskTop->insert(w);
        const auto& hk = resident::hooks();
        std::string id = hk.idOf ? hk.idOf(w) : std::string();
        return "{\"ok\":true,\"id\":\"" + jsonEsc(id) + "\"}";
    }
    if (name == "cast_control" || name == "cast_state") {
        std::string id = str(kv, "id");
        TCastPlayerWindow* w = nullptr;
        if (!id.empty()) {
            const auto& h = resident::hooks();
            w = dynamic_cast<TCastPlayerWindow*>(h.findWindow ? h.findWindow(id) : nullptr);
            if (!w) return "err window " + id + " is not a cast player";
        } else if (TProgram::deskTop && TProgram::deskTop->current) {
            w = dynamic_cast<TCastPlayerWindow*>(TProgram::deskTop->current);
        }
        if (!w) return "err no cast player: give id=";
        if (name == "cast_state") return w->stateJson();
        return w->control(kv);
    }
    *handled = false;
    return "";
}
