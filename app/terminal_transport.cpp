// Terminal transport: the registry commands, the app-level menu command and the act log.
// The process work (SIGSTOP/SIGCONT of the session's groups) is in tvterm_view.cpp.
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TEvent
#include <tvision/tv.h>

#include "terminal_transport.h"
#include "tvterm_view.h"

#include <cstdio>
#include <vector>

namespace termtransport {
namespace {

std::string str(const std::map<std::string, std::string>& kv, const char* k) {
    auto it = kv.find(k);
    return it == kv.end() ? std::string() : it->second;
}

std::string jsonEsc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char) c; }
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += (char) c;
    }
    return o;
}

const char* stateName(TWibWobTerminalWindow::RunState s) {
    switch (s) {
    case TWibWobTerminalWindow::RunState::Paused: return "paused";
    case TWibWobTerminalWindow::RunState::Exited: return "exited";
    default: return "running";
    }
}

std::string idOf(TWibWobTerminalWindow* w) {
    const auto& h = resident::hooks();
    return h.idOf ? h.idOf(w) : std::string();
}

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
    if (name == "terminal_state") {
        std::string err;
        TWibWobTerminalWindow* t = target(kv, err);
        if (!t) return err;
        return stateJson(t);
    }
    *handled = false;
    return "";
}
