// Residents inside the machine. See resident_hands.h and docs/resident-world.md.
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TWindow
#include <tvision/tv.h>

#include "resident_hands.h"
#include "resident_senses.h"
#include "describe.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace resident {

namespace {

using Clock = std::chrono::steady_clock;

std::string jsonStr(const std::string& resp, const char* key) {
    std::string k = std::string("\"") + key + "\":\"";
    size_t p = resp.find(k);
    if (p == std::string::npos) return "";
    size_t e = resp.find('"', p + k.size());
    return e == std::string::npos ? "" : resp.substr(p + k.size(), e - p - k.size());
}

// ---------------------------------------------------------------- reflexes

struct Reflex {
    bool start = false;          // a start line: runs once when the script loads
    std::string kind;            // gesture kind (typed, opened, closed, moved, menu, command, ...)
    std::string type;            // only gestures in a window of this type ("" any)
    std::string actor;
    std::string bind;            // start lines: name the window the command opened
    std::string line;            // the command line, with {placeholders}
};

std::vector<Reflex> g_reflexes;
std::map<std::string, std::string> g_bound;   // bind name -> window id
std::string g_script;
bool g_started = false;
uint64_t g_seen = 0;           // the last gesture looked at
int g_fired = 0;
std::vector<std::string> g_log;   // the last results, newest last

// Typing settles before a reflex answers it: the run's act and when it last grew.
struct Pending { uint64_t g = 0; std::string json; Clock::time_point last{}; };
std::map<uint64_t, Pending> g_typing;   // act -> its latest gesture

std::string field(const std::string& json, const char* key) {
    std::string k = std::string("\"") + key + "\":";
    size_t p = json.find(k);
    if (p == std::string::npos) return "";
    p += k.size();
    if (p < json.size() && json[p] == '"') {
        std::string out;
        for (size_t i = p + 1; i < json.size(); ++i) {
            char c = json[i];
            if (c == '\\' && i + 1 < json.size()) {
                char n = json[++i];
                out += n == 'n' ? '\n' : n == 't' ? '\t' : n;
            } else if (c == '"') break;
            else out += c;
        }
        return out;
    }
    size_t e = json.find_first_of(",}", p);
    return json.substr(p, e - p);
}

std::string expand(const std::string& line, const std::map<std::string, std::string>& vars) {
    std::string out;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '{') {
            size_t e = line.find('}', i);
            if (e != std::string::npos) {
                std::string name = line.substr(i + 1, e - i - 1);
                auto it = vars.find(name);
                if (it != vars.end()) { out += it->second; i = e; continue; }
                auto b = g_bound.find(name);
                if (b != g_bound.end()) { out += b->second; i = e; continue; }
            }
        }
        out += line[i];
    }
    return out;
}

void note(const std::string& s) {
    g_log.push_back(s);
    if (g_log.size() > 50) g_log.erase(g_log.begin());
}

std::string runLine(const Reflex& r, const std::map<std::string, std::string>& vars) {
    std::string name;
    KV kv;
    std::string line = expand(r.line, vars);
    if (!parseCommandLine(line, name, kv)) { note(r.actor + ": cannot read: " + line); return ""; }
    std::string resp = actAs(r.actor, name, kv);
    ++g_fired;
    std::string head = resp.substr(0, std::min<size_t>(resp.size(), 80));
    while (!head.empty() && (head.back() == '\n' || head.back() == '\r')) head.pop_back();
    note(r.actor + ": " + name + " -> " + head);
    return resp;
}

bool boundWindow(const std::string& id) {
    for (auto& b : g_bound) if (b.second == id) return true;
    return false;
}

void answer(const std::string& g) {
    std::string kind = field(g, "kind"), window = field(g, "window"), type = field(g, "type");
    if (boundWindow(window)) return;   // the residents' own windows: the person writing there is not answered
    std::map<std::string, std::string> vars = {
        {"window", window}, {"type", type}, {"what", field(g, "what")}, {"text", field(g, "text")}, {"kind", kind}};
    for (const auto& r : g_reflexes) {
        if (r.start || r.kind != kind) continue;
        if (!r.type.empty() && r.type != type) continue;
        runLine(r, vars);
    }
}

}  // namespace

std::string actAs(const std::string& actor, const std::string& name, KV kv) {
    if (!hooks().exec) return "err no app";
    kv["actor"] = actor;
    kv["source"] = "resident";
    // A window a resident opens must not take the person's focus: put the person's window back as
    // current without changing the z-order, as resident input does for its own dispatches.
    TDeskTop* d = TProgram::deskTop;
    TView* before = d ? d->current : nullptr;
    std::string resp = hooks().exec(name, kv);   // the socket's dispatcher in process: logs the act in actor's name
    if (d && before && d->current != before) {
        bool alive = false;
        if (TView* f = d->first()) { TView* v = f; do { if (v == before) alive = true; v = v->next; } while (v != f); }
        if (alive) d->setCurrent(before, TView::normalSelect);
    }
    return resp;
}

bool parseCommandLine(const std::string& line, std::string& name, KV& kv) {
    size_t i = 0, n = line.size();
    auto skip = [&] { while (i < n && (line[i] == ' ' || line[i] == '\t')) ++i; };
    skip();
    size_t s = i;
    while (i < n && line[i] != ' ' && line[i] != '\t') ++i;
    name = line.substr(s, i - s);
    if (name.rfind("cmd:", 0) == 0) name = name.substr(4);
    if (name.empty()) return false;
    while (true) {
        skip();
        if (i >= n) break;
        size_t ks = i;
        while (i < n && line[i] != '=' && line[i] != ' ') ++i;
        if (i >= n || line[i] != '=') return false;
        std::string key = line.substr(ks, i - ks);
        ++i;
        std::string val;
        if (i < n && line[i] == '"') {
            ++i;
            while (i < n && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < n) {
                    char c = line[++i];
                    val += c == 'n' ? '\n' : c == 't' ? '\t' : c;
                } else val += line[i];
                ++i;
            }
            if (i >= n) return false;
            ++i;
        } else {
            while (i < n && line[i] != ' ' && line[i] != '\t') val += line[i++];
        }
        kv[key] = val;
    }
    return true;
}

bool loadReflexes(const std::string& path, std::string& err) {
    std::ifstream in(path);
    if (!in) { err = "cannot open " + path; return false; }
    std::vector<Reflex> rs;
    std::string raw;
    int ln = 0;
    while (std::getline(in, raw)) {
        ++ln;
        size_t a = raw.find_first_not_of(" \t");
        if (a == std::string::npos || raw[a] == '#') continue;
        std::string l = raw.substr(a);
        size_t colon = l.find(": ");
        if (colon == std::string::npos) { err = path + ":" + std::to_string(ln) + ": no \": \""; return false; }
        std::istringstream head(l.substr(0, colon));
        Reflex r;
        r.line = l.substr(colon + 2);
        std::string w, prev;
        head >> w;
        if (w == "start") r.start = true;
        else if (w == "on") head >> r.kind;
        else { err = path + ":" + std::to_string(ln) + ": a line starts with on or start"; return false; }
        while (head >> w) {
            if (w.rfind("type=", 0) == 0) r.type = w.substr(5);
            else if (w == "as") head >> r.actor;
            else if (w == "->") head >> r.bind;
            else { err = path + ":" + std::to_string(ln) + ": unknown word " + w; return false; }
        }
        if (r.actor.empty()) { err = path + ":" + std::to_string(ln) + ": no actor (as wib|wob|scramble)"; return false; }
        rs.push_back(r);
    }
    g_reflexes = rs;
    g_script = path;
    g_started = false;
    g_bound.clear();
    g_typing.clear();
    g_seen = gestureLast();   // answer gestures from now on
    note("loaded " + path + " (" + std::to_string(rs.size()) + " lines)");
    return true;
}

void tickReflexes() {
    if (g_reflexes.empty()) return;
    if (!g_started) {
        g_started = true;
        for (const auto& r : g_reflexes) {
            if (!r.start) continue;
            std::string resp = runLine(r, {});
            std::string id = jsonStr(resp, "id");
            if (!r.bind.empty() && !id.empty()) g_bound[r.bind] = id;
        }
        g_seen = gestureLast();   // the start lines' own consequences are not the person's
        return;
    }
    uint64_t last = gestureLast();
    while (g_seen < last) {
        std::string g = gestureJson(++g_seen);
        if (g.empty()) continue;
        if (field(g, "kind") == "typed") {   // wait for the run to pause
            uint64_t act = std::strtoull(field(g, "act").c_str(), nullptr, 10);
            g_typing[act] = Pending{g_seen, g, Clock::now()};
            continue;
        }
        if (field(g, "update") == "true") continue;   // drags and moves: answer the first gesture only
        answer(g);
    }
    auto now = Clock::now();
    for (auto it = g_typing.begin(); it != g_typing.end();) {
        if (now - it->second.last > std::chrono::milliseconds(700)) {
            std::string g = gestureJson(it->second.g);
            answer(g.empty() ? it->second.json : g);
            it = g_typing.erase(it);
        } else ++it;
    }
}

std::string cmdReflexes(const KV& kv) {
    auto it = kv.find("load");
    if (it != kv.end()) {
        // Loading a script decides how the residents behave: a rule (the console with the key turned).
        std::string refusal = ruleGate("resident reflexes from " + it->second);
        if (!refusal.empty()) return refusal;
        std::string err;
        if (!loadReflexes(it->second, err)) return "err " + err;
    }
    if (kv.count("clear")) {
        std::string refusal = ruleGate("clear the resident reflexes");
        if (!refusal.empty()) return refusal;
        g_reflexes.clear();
        g_bound.clear();
    }
    std::ostringstream os;
    auto q = [](const std::string& s) {
        std::string o = "\"";
        for (char c : s) {
            if (c == '"' || c == '\\') o += '\\';
            if (c == '\n') { o += "\\n"; continue; }
            o += c;
        }
        return o + "\"";
    };
    os << "{\"script\":" << q(g_script) << ",\"lines\":" << g_reflexes.size() << ",\"fired\":" << g_fired
       << ",\"seen\":" << g_seen << ",\"bound\":{";
    bool first = true;
    for (auto& b : g_bound) { os << (first ? "" : ",") << q(b.first) << ":" << q(b.second); first = false; }
    os << "},\"log\":[";
    for (size_t i = 0; i < g_log.size(); ++i) os << (i ? "," : "") << q(g_log[i]);
    os << "]}";
    return os.str();
}

bool pawCommandAllowed(const std::string& name) {
    static const char* ok[] = {"resident_key", "resident_mouse", "resident_press", "resident_menu", "describe",
                               "look_glance", "look_read", "create_window", "move_window", "resize_window",
                               "close_window", "window_title",
                               "open_synth", "synth", "hear",   // the machine's sound (app/sound)
                               "terminal_pause", "terminal_resume", "terminal_toggle", "terminal_state"};
    for (const char* n : ok) if (name == n) return true;
    return false;
}

std::string takeActs(const std::string& reply, const std::string& actor, std::vector<std::string>& results) {
    std::istringstream in(reply);
    std::string line, kept, opened;   // opened: the window the last act opened (id=new names it)
    while (std::getline(in, line)) {
        size_t a = line.find_first_not_of(" \t");
        if (a != std::string::npos && line.compare(a, 4, "ACT:") == 0) {
            std::string name;
            KV kv;
            if (!parseCommandLine(line.substr(a + 4), name, kv)) { results.push_back("unreadable: " + line); continue; }
            if (!pawCommandAllowed(name)) { results.push_back(name + ": not one of the paws' commands"); continue; }
            bool unknownNew = false;
            for (auto& p : kv)
                if (p.second == "new" && (p.first == "id" || p.first == "window_id")) {
                    if (opened.empty()) unknownNew = true;
                    p.second = opened;
                }
            if (unknownNew) { results.push_back(name + ": id=new, but no window was opened before it"); continue; }
            std::string resp = actAs(actor, name, kv);
            std::string id = jsonStr(resp, "id");
            if (name == "create_window" && !id.empty()) opened = id;
            results.push_back(name + " -> " + resp.substr(0, std::min<size_t>(resp.size(), 80)));
            continue;
        }
        kept += line + "\n";
    }
    while (!kept.empty() && (kept.back() == '\n' || kept.back() == ' ')) kept.pop_back();
    return kept;
}

std::string deskForPrompt() {
    std::ostringstream os;
    TDeskTop* d = TProgram::deskTop;
    if (!d || !d->first()) return "(no windows are open)";
    TView* f = d->first();
    TView* v = f;
    int n = 0;
    do {
        if (auto* w = dynamic_cast<TWindow*>(v)) {
            std::string state, cls;
            std::vector<std::string> text;
            bool model = false;
            const char* t = w->getTitle(0);
            std::string id = hooks().idOf ? hooks().idOf(w) : "";
            std::string type = hooks().typeOf ? hooks().typeOf(w) : "window";
            if (!ww::describeWindow(w, state, text, model, cls)) state = "a " + type + " window";
            os << "- " << id << " " << type << (t && *t ? std::string(" \"") + t + "\"" : "") << " at " << w->origin.x << ","
               << w->origin.y << " " << w->size.x << "x" << w->size.y << ": " << state << "\n";
            ++n;
        }
        v = v->next;
    } while (v != f && n < 40);
    return n ? os.str() : "(no windows are open)";
}

// ---------------------------------------------------------------- rehearsal desk

namespace {
std::map<std::string, std::string> g_rehearsal;   // actor -> its rehearsal window id

std::string exePath() {
#ifdef __APPLE__
    char buf[4096];
    uint32_t n = sizeof buf;
    if (_NSGetExecutablePath(buf, &n) == 0) return buf;
#endif
    char b[4096];
    ssize_t k = readlink("/proc/self/exe", b, sizeof b - 1);
    if (k > 0) { b[k] = 0; return b; }
    return "wwdos";
}
}  // namespace

std::string cmdRehearse(const KV& kv) {
    Who w = currentWho();
    std::string actor = w.source == Source::Console ? "person" : w.actor;
    std::string inst = "rehearse_" + actor;
    std::string sock = "/tmp/wibwob_" + inst + ".sock";
    auto it = kv.find("close");
    if (it != kv.end() && it->second == "1") {
        auto r = g_rehearsal.find(actor);
        if (r == g_rehearsal.end()) return "err no rehearsal desk is open for " + actor;
        std::string resp = actAs(actor, "close_window", {{"id", r->second}});
        g_rehearsal.erase(r);
        return "{\"closed\":true,\"socket\":\"" + sock + "\"}";
    }
    if (g_rehearsal.count(actor)) {
        return "{\"open\":true,\"window\":\"" + g_rehearsal[actor] + "\",\"socket\":\"" + sock + "\"}";
    }
    // A second WIBWOB-DOS in a terminal window the resident owns, with its own socket: what it tries
    // there touches nothing on the public desk, and the person sees the tries in the window.
    std::string exec = "env -u WWDOS_RESIDENTS WIBWOB_INSTANCE=" + inst + " WWDOS_REHEARSAL=1 WWDOS_TR808_AUDIO=0 '" +
                       exePath() + "' 2>/tmp/wibwob_" + inst + ".stderr";
    KV args{{"exec", exec}, {"title", "rehearsal: " + actor}};
    for (const char* k : {"x", "y", "w", "h"}) { auto a = kv.find(k); if (a != kv.end()) args[k] = a->second; }
    if (!args.count("x")) { args["x"] = "4"; args["y"] = "2"; args["w"] = "84"; args["h"] = "26"; }
    std::string resp = w.source == Source::Console ? hooks().exec("open_terminal", args) : actAs(actor, "open_terminal", args);
    std::string id = jsonStr(resp, "id");
    if (id.empty()) return "err the rehearsal desk did not open: " + resp.substr(0, 80);
    g_rehearsal[actor] = id;
    return "{\"open\":true,\"window\":\"" + id + "\",\"socket\":\"" + sock +
           "\",\"note\":\"send commands to this socket; what happens there stays there; resident_rehearse close=1 ends it\"}";
}

}  // namespace resident
