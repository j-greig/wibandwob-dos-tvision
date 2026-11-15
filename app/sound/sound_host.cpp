#include "sound_host.h"

#define Uses_TProgram
#define Uses_TDeskTop
#include <tvision/tv.h>

#include "resident_senses.h"
#include "synth_view.h"
#include "tr808_audio.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sound {

static Host g_host;
void setHost(const Host& h) { g_host = h; }
const Host& host() { return g_host; }

double clockMs() {
    using namespace std::chrono;
    static const steady_clock::time_point t0 = steady_clock::now();
    return duration<double, std::milli>(steady_clock::now() - t0).count();
}

std::string currentActor() {
    std::string a = resident::currentWho().actor;
    return a.empty() ? std::string("app") : a;
}

// ── Helpers ───────────────────────────────────────────────────────────────

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c == '\n') o += "\\n";
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o;
}

std::string kvGet(const KV& kv, const char* k, const std::string& dflt) {
    auto it = kv.find(k);
    return it == kv.end() ? dflt : it->second;
}

bool kvHas(const KV& kv, const char* k) {
    auto it = kv.find(k);
    return it != kv.end() && !it->second.empty();
}

int toInt(const std::string& s, int dflt) {
    if (s.empty()) return dflt;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    return (end && end != s.c_str()) ? (int)v : dflt;
}

bool truthy(const std::string& s) { return s == "1" || s == "true" || s == "on" || s == "yes"; }

// ── Audio out ─────────────────────────────────────────────────────────────

static std::atomic<bool> g_audio{[] {
    const char* e = std::getenv("WWDOS_SYNTH_AUDIO");
    return !(e && (e[0] == '0' || e[0] == 'n' || e[0] == 'N' || e[0] == 'f' || e[0] == 'F'));
}()};

bool synthAudioEnabled() { return g_audio.load(); }
void setSynthAudioEnabled(bool on) { g_audio.store(on); }
bool synthAudioAvailable() { return tr808::playerAvailable(); }

static void removeCache();

static std::string cacheDir() {
    static const std::string dir = [] {
        const char* t = std::getenv("TMPDIR");
        std::string base = (t && *t) ? t : "/tmp";
        if (!base.empty() && base.back() == '/') base.pop_back();
        std::string d = base + "/wwdos-synth-" + std::to_string((long)::getpid());
        ::mkdir(d.c_str(), 0700);
        std::atexit(removeCache);
        return d;
    }();
    return dir;
}

// The rendered phrases are scratch: removed when the app exits normally.
static void removeCache() {
    const std::string d = cacheDir();
    if (DIR* dp = ::opendir(d.c_str())) {
        while (struct dirent* e = ::readdir(dp))
            if (e->d_name[0] != '.') ::unlink((d + "/" + e->d_name).c_str());
        ::closedir(dp);
    }
    ::rmdir(d.c_str());
}

bool sendToSpeakers(const std::vector<Note>& notes) {
    if (!synthAudioEnabled() || !synthAudioAvailable() || notes.empty()) return false;
    // One file per phrase: the phrase keeps its own timing inside the file.
    std::vector<Note> shifted = notes;
    double first = shifted[0].atMs;
    for (const auto& n : shifted) first = std::min(first, n.atMs);
    for (auto& n : shifted) n.atMs -= first;
    auto s = render(shifted, renderLengthMs(shifted) + 5);
    char name[40];
    std::snprintf(name, sizeof(name), "/%016llx.wav", (unsigned long long)fnv1a(s));
    const std::string path = cacheDir() + name;
    struct stat st;
    if (::stat(path.c_str(), &st) != 0 && !writeWav(path, s)) return false;
    tr808::playFile(path, 1.0);
    return true;
}

// ── Windows ───────────────────────────────────────────────────────────────

template <class W>
static W* frontMost() {
    TDeskTop* d = TProgram::deskTop;
    if (!d) return nullptr;
    TView* start = d->first();
    if (!start) return nullptr;
    TView* v = start;
    do {
        if (auto* w = dynamic_cast<W*>(v)) return w;
        v = v->next;
    } while (v != start);
    return nullptr;
}

template <class W>
static W* findOrFront(const KV& kv) {
    const std::string id = kvGet(kv, "id");
    if (!id.empty() && id != "auto") return g_host.find ? dynamic_cast<W*>(g_host.find(id)) : nullptr;
    return frontMost<W>();
}

static TRect boundsFrom(const KV& kv, int w, int h) {
    if (kvHas(kv, "x") && kvHas(kv, "y") && kvHas(kv, "w") && kvHas(kv, "h")) {
        const int x = toInt(kvGet(kv, "x"), 0), y = toInt(kvGet(kv, "y"), 0);
        return TRect(x, y, x + toInt(kvGet(kv, "w"), w), y + toInt(kvGet(kv, "h"), h));
    }
    TRect r(0, 0, w, h);
    if (TDeskTop* d = TProgram::deskTop) {
        const int dx = std::max(0, (d->size.x - w) / 2), dy = std::max(0, (d->size.y - h) / 2);
        r.move(dx, dy);
    }
    return r;
}

static std::string withId(TWindow* w, const std::string& json) {
    const std::string id = g_host.idOf ? g_host.idOf(w) : "";
    if (json.empty() || json[0] != '{') return json;
    return "{\"id\":\"" + id + "\"," + json.substr(1);
}

static std::string openSynth(const KV& kv) {
    TWindow* w = createSynthWindow(boundsFrom(kv, 60, 18), kv);
    const std::string id = g_host.insert ? g_host.insert(w) : "";
    return "{\"ok\":true,\"id\":\"" + id + "\"}";
}

bool handles(const std::string& name) {
    return name == "open_synth" || name == "synth";
}

std::string command(const std::string& name, const KV& kv) {
    if (name == "open_synth") return openSynth(kv);
    if (name == "synth") {
        const std::string action = kvGet(kv, "action");
        auto* w = findOrFront<TSynthWindow>(kv);
        if (!w) {
            const std::string id = kvGet(kv, "id");
            if (!id.empty() && id != "auto") return "err synth window not found";
            if (action != "open" && action != "note" && action != "notes" && action != "play")
                return "err no synth window (action=open opens one)";
            openSynth({});
            w = frontMost<TSynthWindow>();
            if (!w) return "err synth spawn failed";
        }
        return withId(w, w->view()->apiAction(action, kv));
    }
    return "err unknown sound command";
}

std::string stateOf(TWindow* w) {
    if (auto* s = dynamic_cast<TSynthWindow*>(w)) return s->view()->sentence();
    return "";
}

}  // namespace sound

// The registry's entry point (declared weak in command_registry.cpp).
std::string sound_registry_command(const std::string& name, const std::map<std::string, std::string>& kv,
                                   bool* handled) {
    *handled = sound::handles(name);
    return *handled ? sound::command(name, kv) : std::string();
}
