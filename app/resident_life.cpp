// Resident life: the self on disk and waking as a boot (docs/resident-life.md).
#include "resident_life.h"

#include "core/json_utils.h"
#include "resident_senses.h"

#define Uses_TWindow
#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TDeskTop
#define Uses_TProgram
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TStatusLine
#include <tvision/tv.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <sys/stat.h>

namespace life {
namespace {

using resident::Source;
using resident::Who;

Hooks g_hooks;

// ---------------------------------------------------------------- small helpers

long long nowMs() {
    using namespace std::chrono;
    static const steady_clock::time_point t0 = steady_clock::now();
    return duration_cast<milliseconds>(steady_clock::now() - t0).count();
}

std::string q(const std::string& s) { return "\"" + json_escape(s) + "\""; }
std::string str(const KV& kv, const char* k, const std::string& def = "") {
    auto it = kv.find(k);
    return it == kv.end() ? def : it->second;
}
long envMs(const char* name, long def) {
    const char* v = std::getenv(name);
    if (!v || !*v) return def;
    return std::strtol(v, nullptr, 10);
}

// Dates in the form a period directory listing shows them: SUN 15-11-2026 17:40.
std::string dateTime(std::time_t t) {
    static const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%s %02d-%02d-%04d %02d:%02d", days[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1,
                  tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
    return buf;
}
std::string nowStamp() { return dateTime(std::time(nullptr)); }

bool mkdirs(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == '/' && cur.size() > 1) ::mkdir(cur.c_str(), 0755);
    }
    ::mkdir(path.c_str(), 0755);
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::vector<std::string> readLines(const std::string& path, bool* exists = nullptr) {
    std::vector<std::string> out;
    std::ifstream f(path);
    if (exists) *exists = f.is_open();
    std::string l;
    while (std::getline(f, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        out.push_back(l);
    }
    return out;
}

// Whole-file write through a temporary file and a rename, so a crash leaves the old file.
bool writeFile(const std::string& path, const std::string& text) {
    std::string tmp = path + ".TMP";
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f) return false;
        f << text;
        if (!f) return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

bool appendFile(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::app);
    if (!f) return false;
    f << text;
    return (bool)f;
}

std::string upper(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

// One line of text from untrusted input: no control characters, at most n bytes.
std::string oneLine(const std::string& s, size_t n) {
    std::string out;
    for (unsigned char c : s) {
        if (c == '\n' || c == '\r' || c == '\t') out += ' ';
        else if (c >= 0x20 && c != 0x7f) out += (char)c;
    }
    if (out.size() > n) {
        out.resize(n);
        while (!out.empty() && (out.back() & 0xC0) == 0x80) out.pop_back();   // whole UTF-8 characters
        if (!out.empty() && (unsigned char)out.back() >= 0xC0) out.pop_back();
    }
    return out;
}

std::string padDots(const std::string& s, size_t w) {
    std::string out = s + " ";
    while (out.size() < w) out += '.';
    return out + " ";
}

const char* kResidents[] = {"wib", "wob", "scramble"};
bool isResident(const std::string& a) {
    for (auto r : kResidents) if (a == r) return true;
    return false;
}
std::string displayName(const std::string& a) {
    if (a == "wib") return "Wib";
    if (a == "wob") return "Wob";
    if (a == "scramble") return "Scramble";
    return a;
}

// ---------------------------------------------------------------- the self on disk

struct DeskLine {
    std::string id, type, owner, title;
    int x = 0, y = 0, w = 0, h = 0;
};

struct Self {
    bool found = false;        // SELF.INI existed at waking
    int boots = 0;             // this waking's number
    std::string lastWake;      // the previous waking
    std::string lastClose;     // when the previous waking closed ("" if it did not)
    bool lastClean = true;     // the previous waking closed through close()
    std::string wakeStamp;     // this waking
    std::vector<DeskLine> lastDesk;
    bool deskFound = false;
};
Self g_self;
bool g_woken = false;
bool g_closed = false;

std::string g_home;

std::string path(const std::string& name) { return home() + "/" + name; }

std::map<std::string, std::string> readIni(const std::string& file, bool* exists) {
    std::map<std::string, std::string> kv;
    std::string section;
    for (auto& l : readLines(file, exists)) {
        if (l.empty() || l[0] == ';') continue;
        if (l[0] == '[') {
            size_t e = l.find(']');
            section = l.substr(1, e == std::string::npos ? std::string::npos : e - 1);
            continue;
        }
        size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        kv[section + "." + l.substr(0, eq)] = l.substr(eq + 1);
    }
    return kv;
}

void writeSelfIni(bool running) {
    std::ostringstream os;
    os << "; WIBWOB-DOS self. Written by the app; read at every waking.\n"
       << "; boots: wakings so far. closed: clean when the last waking ended through Exit,\n"
       << "; running while a waking is in progress (left so when the app is killed).\n"
       << "[self]\n"
       << "boots=" << g_self.boots << "\n"
       << "last_wake=" << g_self.wakeStamp << "\n"
       << "last_close=" << (running ? g_self.lastClose : nowStamp()) << "\n"
       << "closed=" << (running ? "running" : "clean") << "\n";
    writeFile(path("SELF.INI"), os.str());
}

// DESK.TXT: one window per line, tab-separated: id, type, owner, x,y,w,h, title.
std::vector<DeskLine> readDesk(bool* exists) {
    std::vector<DeskLine> out;
    for (auto& l : readLines(path("DESK.TXT"), exists)) {
        if (l.empty() || l[0] == ';') continue;
        std::vector<std::string> f;
        size_t p = 0;
        while (true) {
            size_t t = l.find('\t', p);
            f.push_back(l.substr(p, t == std::string::npos ? std::string::npos : t - p));
            if (t == std::string::npos) break;
            p = t + 1;
        }
        if (f.size() < 5) continue;
        DeskLine d;
        d.id = f[0];
        d.type = f[1];
        d.owner = f[2];
        std::sscanf(f[3].c_str(), "%d,%d,%d,%d", &d.x, &d.y, &d.w, &d.h);
        d.title = f[4];
        out.push_back(d);
    }
    return out;
}

std::vector<TWindow*> deskWindows() {
    std::vector<TWindow*> out;
    if (!TProgram::deskTop) return out;
    TView* first = TProgram::deskTop->first();
    if (!first) return out;
    TView* v = first;
    do {
        if (auto* w = dynamic_cast<TWindow*>(v)) out.push_back(w);
        v = v->next;
    } while (v && v != first);
    return out;
}

std::string titleOf(TWindow* w) {
    const char* t = w->getTitle(0);
    return t ? t : "";
}

void writeDesk() {
    std::ostringstream os;
    os << "; WIBWOB-DOS desk at the last close: " << nowStamp() << "\n"
       << "; id<TAB>type<TAB>owner<TAB>x,y,w,h<TAB>title (front window first)\n";
    for (TWindow* w : deskWindows()) {
        if (isBootWindow(w) || isDreamWindow(w)) continue;
        TRect r = w->getBounds();
        std::string owner = resident::ownerName(w);
        os << resident::hooks().idOf(w) << "\t" << resident::hooks().typeOf(w) << "\t"
           << (owner.empty() ? "app" : owner) << "\t" << r.a.x << "," << r.a.y << "," << (r.b.x - r.a.x) << ","
           << (r.b.y - r.a.y) << "\t" << oneLine(titleOf(w), 60) << "\n";
    }
    writeFile(path("DESK.TXT"), os.str());
}

// ---------------------------------------------------------------- the boot screen

class TBootView : public TView {
public:
    TBootView(const TRect& r) : TView(r) { growMode = gfGrowHiX | gfGrowHiY; }
    std::vector<std::string> lines;
    std::vector<bool> loud;   // drawn bright (warnings, the last line)
    size_t shown = 0;
    void draw() override {
        TColorAttr norm(TColorBIOS(7), TColorBIOS(0)), bright(TColorBIOS(15), TColorBIOS(0));
        size_t n = std::min(shown, lines.size());
        size_t top = n > (size_t)size.y ? n - size.y : 0;   // the newest lines stay in view
        for (int y = 0; y < size.y; ++y) {
            TDrawBuffer b;
            size_t i = top + y;
            bool isLoud = i < n && i < loud.size() && loud[i];
            b.moveChar(0, ' ', norm, size.x);
            if (i < n) b.moveStr(1, lines[i].c_str(), isLoud ? bright : norm);
            writeLine(0, y, size.x, 1, b);
        }
    }
};

class TBootWindow : public TWindow {
public:
    TBootWindow(const TRect& r, const std::string& title)
        : TWindowInit(&TBootWindow::initFrame), TWindow(r, title.c_str(), wnNoNumber) {
        options |= ofTileable;
        TRect in = getExtent();
        in.grow(-1, -1);
        view = new TBootView(in);
        insert(view);
    }
    TBootView* view;
};

TBootWindow* g_boot = nullptr;
long long g_bootNext = 0;      // when the next line appears
long long g_bootDoneMs = -1;   // when the last line appeared
long g_bootLineMs = 120;
long g_bootHoldMs = 20000;

bool bootAlive() {
    if (!g_boot) return false;
    for (TWindow* w : deskWindows()) if (w == g_boot) return true;
    g_boot = nullptr;
    return false;
}

void addBoot(std::vector<std::string>& lines, std::vector<bool>& loud, const std::string& s, bool isLoud = false) {
    lines.push_back(s);
    loud.push_back(isLoud);
}

void bootLines(std::vector<std::string>& lines, std::vector<bool>& loud) {
    addBoot(lines, loud, "WIBWOB-DOS BIOS  Version 1.0   (C) Wib&Wob");
    addBoot(lines, loud, "Memory test: 640K OK");
    addBoot(lines, loud, "Self on disk: C:\\WWDOS  = " + home());
    addBoot(lines, loud, "Waking " + std::to_string(g_self.boots) + ".  " + g_self.wakeStamp);
    if (!g_self.found) {
        addBoot(lines, loud, "No self found. First waking.");
    } else if (!g_self.lastClean) {
        addBoot(lines, loud, "WARNING: the last waking (" + g_self.lastWake + ") did not close.", true);
        addBoot(lines, loud, "         Power lost or killed; its notes and desk were not written.", true);
    } else {
        addBoot(lines, loud, "Last waking: " + g_self.lastWake + ", closed cleanly " + g_self.lastClose + ".");
    }
    addBoot(lines, loud, padDots("DEVICE=EYES.SYS", 22) + "look_glance look_read look_study look_changes");
    addBoot(lines, loud, padDots("DEVICE=HANDS.SYS", 22) + "resident_key resident_mouse resident_press resident_menu");
    bool model = g_hooks.modelPresent && g_hooks.modelPresent();
    addBoot(lines, loud, padDots("DEVICE=VOICE.SYS", 22) +
                             (model ? "model present, confined calls only"
                                    : "no model; the residents speak by rule"));
    for (auto r : kResidents) {
        int mine = 0;
        for (auto& d : g_self.lastDesk) if (d.owner == r) ++mine;
        std::string s = padDots(upper(r), 14);
        s += mine ? std::to_string(mine) + (mine == 1 ? " window was mine" : " windows were mine") + " at the last close"
                  : "no windows of mine at the last close";
        addBoot(lines, loud, s);
    }
    if (g_self.deskFound) {
        addBoot(lines, loud, "Windows at the last close: " + std::to_string(g_self.lastDesk.size()));
        size_t n = 0;
        for (auto& d : g_self.lastDesk) {
            if (++n > 8) {
                addBoot(lines, loud, "  ... and " + std::to_string(g_self.lastDesk.size() - 8) + " more");
                break;
            }
            char buf[200];
            std::snprintf(buf, sizeof(buf), "  %-5s %-14s %-9s %d,%d %dx%d  \"%s\"", d.id.c_str(), d.type.c_str(),
                          d.owner.c_str(), d.x, d.y, d.w, d.h, oneLine(d.title, 30).c_str());
            addBoot(lines, loud, buf);
        }
    }
    addBoot(lines, loud, "");
    addBoot(lines, loud, "Human not present. Press any key to continue", true);
}

void openBoot() {
    if (!TProgram::deskTop) return;
    std::vector<std::string> lines;
    std::vector<bool> loud;
    bootLines(lines, loud);
    TRect d = TProgram::deskTop->getExtent();
    int w = std::min(d.b.x - 2, 78);
    int h = std::min(d.b.y - 1, (int)lines.size() + 2);
    if (w < 20 || h < 4) return;
    int x = std::max(0, (d.b.x - w) / 2), y = std::max(0, (d.b.y - h) / 2);
    auto* bw = new TBootWindow(TRect(x, y, x + w, y + h), "BOOT");
    bw->view->lines = lines;
    bw->view->loud = loud;
    bw->view->shown = g_bootLineMs <= 0 ? lines.size() : 0;
    g_boot = bw;
    if (g_hooks.insertWindow) g_hooks.insertWindow(bw);
    else TProgram::deskTop->insert(bw);
    g_bootNext = nowMs() + g_bootLineMs;
    g_bootDoneMs = bw->view->shown >= lines.size() ? nowMs() : -1;
}

void closeBoot() {
    if (!bootAlive()) return;
    TWindow* w = g_boot;
    g_boot = nullptr;
    w->close();
}

void tickBoot() {
    if (!bootAlive()) return;
    TBootView* v = g_boot->view;
    long long t = nowMs();
    if (v->shown < v->lines.size()) {
        if (t >= g_bootNext) {
            ++v->shown;
            g_bootNext = t + g_bootLineMs;
            v->drawView();
            if (v->shown >= v->lines.size()) g_bootDoneMs = t;
        }
        return;
    }
    if (g_bootHoldMs > 0 && g_bootDoneMs >= 0 && t - g_bootDoneMs >= g_bootHoldMs) {
        resident::WhoScope ws(Who{"app", Source::App});
        closeBoot();
        resident::settle();
    }
}

std::string bootJson() {
    std::ostringstream os;
    bool open = bootAlive();
    os << "{\"open\":" << (open ? "true" : "false");
    if (open) {
        os << ",\"id\":" << q(resident::hooks().idOf(g_boot)) << ",\"shown\":" << g_boot->view->shown
           << ",\"lines\":" << g_boot->view->lines.size();
    }
    os << "}";
    return os.str();
}

std::string deskJson(const std::vector<DeskLine>& desk) {
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < desk.size(); ++i) {
        auto& d = desk[i];
        os << (i ? "," : "") << "{\"id\":" << q(d.id) << ",\"type\":" << q(d.type) << ",\"owner\":" << q(d.owner)
           << ",\"x\":" << d.x << ",\"y\":" << d.y << ",\"w\":" << d.w << ",\"h\":" << d.h
           << ",\"title\":" << q(d.title) << "}";
    }
    os << "]";
    return os.str();
}

std::string cmdSelf(const KV&) {
    std::ostringstream os;
    os << "{\"home\":" << q(home()) << ",\"waking\":" << g_self.boots << ",\"woke\":" << q(g_self.wakeStamp)
       << ",\"found\":" << (g_self.found ? "true" : "false") << ",\"last_wake\":" << q(g_self.lastWake)
       << ",\"last_close\":" << q(g_self.lastClose)
       << ",\"last_closed_cleanly\":" << (g_self.found ? (g_self.lastClean ? "true" : "false") : "null")
       << ",\"desk_at_last_close\":" << deskJson(g_self.lastDesk) << ",\"boot\":" << bootJson() << "}";
    return os.str();
}

}  // namespace

// ---------------------------------------------------------------- public

void setHooks(const Hooks& h) { g_hooks = h; }

std::string home() {
    if (g_home.empty()) {
        const char* h = std::getenv("WWDOS_HOME");
        if (h && *h) g_home = h;
        else {
            const char* u = std::getenv("HOME");
            g_home = std::string(u && *u ? u : ".") + "/.wwdos";
        }
        while (g_home.size() > 1 && g_home.back() == '/') g_home.pop_back();
        mkdirs(g_home);
    }
    return g_home;
}

void wake() {
    if (g_woken) return;
    g_woken = true;
    bool exists = false;
    auto ini = readIni(path("SELF.INI"), &exists);
    g_self.found = exists;
    g_self.boots = std::atoi(ini["self.boots"].c_str()) + 1;
    g_self.lastWake = ini["self.last_wake"];
    g_self.lastClose = ini["self.last_close"];
    g_self.lastClean = !exists || ini["self.closed"] == "clean";
    if (!g_self.lastClean) g_self.lastClose.clear();
    g_self.wakeStamp = nowStamp();
    g_self.lastDesk = readDesk(&g_self.deskFound);
    writeSelfIni(true);

    g_bootLineMs = envMs("WWDOS_BOOT_MS", 120);
    g_bootHoldMs = envMs("WWDOS_BOOT_HOLD_MS", 20000);
    for (auto r : kResidents)
        resident::addAct(Who{r, Source::Resident}, "woke: waking " + std::to_string(g_self.boots), "", "");
    const char* boot = std::getenv("WWDOS_BOOT");
    if (!(boot && std::string(boot) == "0")) {
        resident::WhoScope ws(Who{"app", Source::App});
        openBoot();
        resident::settle();
    }
}

void close(const std::string& how) {
    if (!g_woken || g_closed) return;
    g_closed = true;
    writeDesk();
    writeSelfIni(false);
    resident::addAct(Who{"app", Source::App}, "closed (" + how + "): self written to " + home(), "", "");
}

void tick() { tickBoot(); }

bool consoleInput(TEvent& ev) {
    bool press = (ev.what & (evKeyDown | evMouseDown)) != 0;
    if (press && bootAlive()) {
        TBootView* v = g_boot->view;
        if (v->shown < v->lines.size()) {   // a key during the boot shows the rest at once
            v->shown = v->lines.size();
            g_bootDoneMs = nowMs();
            v->drawView();
        } else {
            closeBoot();
            resident::settle();
        }
        ev.what = evNothing;
        return true;
    }
    return false;
}

bool isBootWindow(TWindow* w) { return w && dynamic_cast<TBootWindow*>(w) != nullptr; }
bool isDreamWindow(TWindow*) { return false; }

bool textOf(TWindow* w, std::vector<std::string>& out) {
    if (auto* b = dynamic_cast<TBootWindow*>(w)) {
        out.assign(b->view->lines.begin(), b->view->lines.begin() + std::min(b->view->shown, b->view->lines.size()));
        return true;
    }
    return false;
}

std::string stateOf(TWindow* w) {
    if (auto* b = dynamic_cast<TBootWindow*>(w)) {
        size_t n = std::min(b->view->shown, b->view->lines.size());
        std::string last = n ? b->view->lines[n - 1] : "";
        return "boot screen of waking " + std::to_string(g_self.boots) + ", " + std::to_string(n) + " of " +
               std::to_string(b->view->lines.size()) + " lines shown" +
               (last.empty() ? "" : "; last line: \"" + oneLine(last, 50) + "\"");
    }
    return "";
}

bool owns(const std::string& cmd) { return cmd == "get_self"; }

std::string command(const std::string& name, const KV& kv) {
    if (name == "get_self") return cmdSelf(kv);
    return "";
}

const char* kaomoji() { return nullptr; }
std::string statusWord() { return ""; }
bool handleCommand(unsigned short) { return false; }

}  // namespace life
