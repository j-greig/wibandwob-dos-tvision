// Errors that teach, and the resident's self-test. See resident_teach.h.
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TWindow
#include <tvision/tv.h>

#include "resident_teach.h"
#include "resident_senses.h"
#include "resident_hands.h"
#include "command_registry.h"
#include "window_type_registry.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <sstream>

namespace resident {

namespace {

std::string q(const std::string& s) {
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        if (c == '\n') { o += "\\n"; continue; }
        o += c;
    }
    return o + "\"";
}

std::string str(const KV& kv, const char* k) {
    auto it = kv.find(k);
    return it == kv.end() ? "" : it->second;
}

size_t distance(const std::string& a, const std::string& b) {
    std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) prev[j] = j;
    for (size_t i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (size_t j = 1; j <= b.size(); ++j)
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

// The rules: commands (and the arguments that make them rules) only the console with the key turned runs.
struct Rule { const char* name; const char* args; };
const Rule kRules[] = {
    {"resident_manners", "share_ms, wait_on_button"}, {"look_mark", "on, ms"}, {"resident_hand", "on, ms"},
    {"window_owner", "owner"}, {"resident_reflexes", "load, clear"},
};

const Rule* ruleOf(const std::string& name) {
    for (const auto& r : kRules) if (name == r.name) return &r;
    return nullptr;
}

}  // namespace

const std::vector<SocketCommand>& socketCommands() {
    static const std::vector<SocketCommand> v = {
        {"get_state", "every window: id, type, rect, z-order, focus, title"},
        {"create_window", "open a window: type= x= y= w= h= title= (get_window_types lists the types)"},
        {"move_window", "id= x= y="}, {"resize_window", "id= width= height="}, {"focus_window", "id="},
        {"close_window", "id="}, {"close_all", "close every window"}, {"cascade", "cascade the windows"},
        {"tile", "tile the windows"}, {"get_canvas_size", "the screen size in cells"},
        {"get_window_types", "every window type and whether it can be opened"},
        {"get_capabilities", "every registry command"}, {"exec_command", "name= runs a registry command"},
        {"send_text", "id= text= mode= into a text editor"}, {"send_figlet", "id= text= font="},
        {"paint_cell", "id= x= y= fg= bg="}, {"paint_text", "id= x= y= text="}, {"paint_line", "id= x0 y0 x1 y1"},
        {"paint_rect", "id= x0 y0 x1 y1"}, {"paint_clear", "id="}, {"paint_export", "id="},
        {"screenshot", "save a screenshot"}, {"save_workspace", "save the layout"}, {"open_workspace", "path="},
        {"move_many", "move several windows"}, {"subscribe_events", "keep the connection: events are pushed"},
        {"batch", "lines=base64:<many command lines>"}, {"pattern_mode", "the desktop pattern mode"},
    };
    return v;
}

std::vector<std::string> allCommandNames() {
    std::vector<std::string> out;
    for (const auto& c : get_command_capabilities()) out.push_back(c.name);
    for (const auto& c : socketCommands()) out.push_back(c.name);
    return out;
}

std::vector<std::string> didYouMean(const std::string& name, const std::vector<std::string>& known) {
    std::vector<std::pair<size_t, std::string>> scored;
    std::string n = name;
    std::replace(n.begin(), n.end(), ' ', '_');
    for (const auto& k : known) {
        size_t d = distance(n, k);
        bool near = d <= std::max<size_t>(2, n.size() / 3);
        bool part = n.size() >= 4 && (k.find(n) != std::string::npos || n.find(k) != std::string::npos);
        if (near || part) scored.push_back({part && !near ? d + 3 : d, k});
    }
    std::sort(scored.begin(), scored.end());
    std::vector<std::string> out;
    for (auto& s : scored) {
        if (std::find(out.begin(), out.end(), s.second) == out.end()) out.push_back(s.second);
        if (out.size() == 3) break;
    }
    return out;
}

static std::string joinNames(const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) s += (i ? ", " : "") + v[i];
    return s;
}

std::string unknownCommand(const std::string& name) {
    auto near = didYouMean(name, allCommandNames());
    std::string s = "err unknown cmd: " + name;
    if (!near.empty()) s += "; did you mean: " + joinNames(near) + "?";
    return s + " (help lists every command; help name=<command> explains one)";
}

std::string unknownWindowType(const std::string& type) {
    std::vector<std::string> types;
    for (const auto& t : all_window_type_specs()) if (t.spawn) types.push_back(t.type);
    auto near = didYouMean(type, types);
    std::string s = "err unknown type: " + type;
    if (!near.empty()) s += "; did you mean: " + joinNames(near) + "?";
    return s + " (get_window_types lists them)";
}

std::string whyNot(const std::string& name, const KV& kv) {
    const Rule* r = ruleOf(name);
    if (!r) return "";
    bool changes = false;
    std::string args = r->args;
    for (const auto& p : kv) if (args.find(p.first) != std::string::npos) changes = true;
    if (!changes) return "";
    Who w = currentWho();
    if (w.source == Source::Console && keyTurned()) return "";
    return std::string(name) + " with " + r->args + " is a rule: only the person at the console, with the console key "
           "turned (Tools > Residents > Console key), changes it; you are " + sourceName(w.source) + " " + w.actor +
           " (standing " + standingOf(w) + "). Without those arguments it only reports.";
}

std::string cmdHelp(const KV& kv) {
    std::string name = str(kv, "name");
    std::ostringstream os;
    Who w = currentWho();
    if (name.empty()) {
        os << "{\"you\":{\"actor\":" << q(w.actor) << ",\"standing\":" << q(standingOf(w)) << "},\"registry\":[";
        bool first = true;
        for (const auto& c : get_command_capabilities()) { os << (first ? "" : ",") << q(c.name); first = false; }
        os << "],\"socket\":[";
        first = true;
        for (const auto& c : socketCommands()) { os << (first ? "" : ",") << q(c.name); first = false; }
        os << "],\"rules\":[";
        first = true;
        for (const auto& r : kRules) { os << (first ? "" : ",") << q(std::string(r.name) + " (" + r.args + ")"); first = false; }
        os << "]}";
        return os.str();
    }
    std::string desc, kind;
    for (const auto& c : get_command_capabilities()) if (name == c.name) { desc = c.description; kind = "registry"; }
    for (const auto& c : socketCommands()) if (name == c.name) { desc = c.description; kind = "socket"; }
    if (kind.empty()) return unknownCommand(name);
    const Rule* r = ruleOf(name);
    KV probe;
    if (r) { std::string a = r->args; probe[a.substr(0, a.find(','))] = "1"; }
    std::string why = r ? whyNot(name, probe) : "";
    os << "{\"name\":" << q(name) << ",\"kind\":" << q(kind) << ",\"description\":" << q(desc)
       << ",\"rule\":" << (r ? q(std::string("changing ") + r->args + " is a rule") : std::string("false"))
       << ",\"you\":{\"actor\":" << q(w.actor) << ",\"source\":" << q(sourceName(w.source))
       << ",\"standing\":" << q(standingOf(w)) << "},\"you_may\":" << (why.empty() ? "true" : "false");
    if (!why.empty()) os << ",\"why_not\":" << q(why);
    os << "}";
    return os.str();
}

// ---------------------------------------------------------------- the self-test

namespace {

struct Check { std::string name; bool ok; std::string detail; };

struct Selftest {
    bool running = false;
    std::string actor;
    std::vector<Check> checks;
    int step = 0;
    std::string win;                 // the editor it opens
    uint64_t waitLast = 0;           // a resident item whose delivery it waits for
    std::chrono::steady_clock::time_point deadline{};
    int x0 = 0;
    long long startedMs = 0;
    long long endedMs = 0;
};
Selftest g_st;

std::string jfield(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\":";
    size_t p = j.find(k);
    if (p == std::string::npos) return "";
    p += k.size();
    if (p < j.size() && j[p] == '"') { size_t e = j.find('"', p + 1); return j.substr(p + 1, e - p - 1); }
    size_t e = j.find_first_of(",}]", p);
    return j.substr(p, e - p);
}

void check(const std::string& name, bool ok, const std::string& detail) { g_st.checks.push_back({name, ok, detail}); }

bool delivered(uint64_t last) {
    std::string s = actAs(g_st.actor, "resident_status", {{"since", std::to_string(last - 1)}});
    return s.find("\"seq\":" + std::to_string(last) + ",") != std::string::npos;   // logged once its delivery starts
}

void finish() {
    g_st.running = false;
    g_st.endedMs = actsClockMs();
    int pass = 0;
    for (auto& c : g_st.checks) pass += c.ok;
    addAct(Who{g_st.actor, Source::Resident}, "self-test: " + std::to_string(pass) + " of " +
           std::to_string(g_st.checks.size()) + " passed", "", pass == (int)g_st.checks.size() ? "" : "some failed");
}

}  // namespace

void tickSelftest() {
    if (!g_st.running) return;
    auto now = std::chrono::steady_clock::now();
    const std::string& a = g_st.actor;
    if (g_st.waitLast) {   // wait until its keys or clicks have landed (or give up)
        if (!delivered(g_st.waitLast)) {
            if (now < g_st.deadline) return;
            check("items delivered", false, "not delivered in 8 s");
        }
        g_st.waitLast = 0;
    }
    auto queue = [&](const std::string& cmd, KV kv) {
        std::string r = actAs(a, cmd, kv);
        std::string last = jfield(r, "last");
        if (!last.empty()) { g_st.waitLast = std::strtoull(last.c_str(), nullptr, 10); g_st.deadline = now + std::chrono::seconds(8); }
        return r;
    };
    switch (g_st.step++) {
    case 0: {
        std::string r = actAs(a, "create_window", {{"type", "text_editor"}, {"x", "3"}, {"y", "2"}, {"w", "40"}, {"h", "8"},
                                                    {"title", "self-test"}});
        g_st.win = jfield(r, "id");
        check("open a text editor", !g_st.win.empty(), r.substr(0, 60));
        if (g_st.win.empty()) { finish(); return; }
        break;
    }
    case 1: queue("resident_key", {{"id", g_st.win}, {"text", "self-test ok"}}); break;
    case 2: {
        std::string d = actAs(a, "describe", {{"id", g_st.win}});
        check("type into it and read the words back from its model", d.find("\"text\":[\"self-test ok\"]") != std::string::npos,
              jfield(d, "state"));
        break;
    }
    case 3: queue("resident_menu", {{"choose", "Help>About WIBWOBWORLD"}}); break;
    case 4: {
        std::string f = actAs(a, "resident_front", {});
        bool dlg = f.find("\"kind\":\"dialog\"") != std::string::npos;
        check("choose a menu item by name and see the dialog it opens", dlg, dlg ? "a dialog is in front" : f.substr(0, 60));
        queue("resident_press", {{"label", "OK"}});
        break;
    }
    case 5: {
        std::string f = actAs(a, "resident_front", {});
        bool closed = f.find("\"modal\":false") != std::string::npos;
        if (!closed) {   // the press is logged when it starts; the dialog closes a pass later
            if (g_st.deadline.time_since_epoch().count() == 0 || now < g_st.deadline + std::chrono::seconds(2)) {
                if (g_st.deadline.time_since_epoch().count() == 0) g_st.deadline = now;
                --g_st.step;
                return;
            }
        }
        check("answer the dialog by its button's label", closed, closed ? "" : "still in front after 2 s");
        std::string d = actAs(a, "describe", {{"id", g_st.win}});
        g_st.x0 = std::atoi(jfield(d, "x").c_str());
        int y = std::atoi(jfield(d, "y").c_str());
        queue("resident_mouse", {{"id", g_st.win}, {"action", "drag"}, {"x", std::to_string(g_st.x0 + 10)},
                                 {"y", std::to_string(y)}, {"x2", std::to_string(g_st.x0 + 22)}, {"y2", std::to_string(y + 3)}});
        break;
    }
    case 6: {
        std::string d = actAs(a, "describe", {{"id", g_st.win}});
        int x1 = std::atoi(jfield(d, "x").c_str());
        check("drag its window by the title bar", x1 == g_st.x0 + 12, "x " + std::to_string(g_st.x0) + " -> " + std::to_string(x1));
        break;
    }
    case 7: {
        std::string r = actAs(a, "resident_hand", {{"on", "0"}});
        check("a rule it may not change is refused, with the reason", r.rfind("err refused", 0) == 0, r.substr(0, 70));
        std::string e = actAs(a, "opn_window", {});
        check("a mistyped command answers with what was meant", e.find("did you mean") != std::string::npos, e.substr(0, 70));
        std::string pe = actAs(a, "person_events", {{"since", "0"}});
        check("see what the person does (person_events)", pe.rfind("{\"last\":", 0) == 0, "");
        break;
    }
    case 8: {
        std::string r = actAs(a, "close_window", {{"id", g_st.win}});
        std::string d = actAs(a, "describe", {{"id", g_st.win}});
        check("close its window", d.rfind("err window not found", 0) == 0, r.substr(0, 40));
        finish();
        return;
    }
    }
}

std::string cmdSelftest(const KV& kv) {
    Who w = currentWho();
    if (str(kv, "start") == "1") {
        if (g_st.running) return "err a self-test is running (resident_selftest reports it)";
        g_st = Selftest{};
        g_st.running = true;
        g_st.actor = (w.source == Source::Console) ? "person" : w.actor;
        g_st.startedMs = actsClockMs();
        addAct(Who{g_st.actor, Source::Resident}, "self-test started", "", "");
    }
    int pass = 0;
    for (auto& c : g_st.checks) pass += c.ok;
    std::ostringstream os;
    os << "{\"actor\":" << q(g_st.actor) << ",\"running\":" << (g_st.running ? "true" : "false")
       << ",\"passed\":" << pass << ",\"failed\":" << (int)g_st.checks.size() - pass << ",\"checks\":[";
    for (size_t i = 0; i < g_st.checks.size(); ++i)
        os << (i ? "," : "") << "{\"check\":" << q(g_st.checks[i].name) << ",\"ok\":" << (g_st.checks[i].ok ? "true" : "false")
           << ",\"detail\":" << q(g_st.checks[i].detail) << "}";
    os << "]}";
    return os.str();
}

}  // namespace resident
