#include "sound_host.h"

#define Uses_TEventQueue
#define Uses_TProgram
#define Uses_TDeskTop
#include <tvision/tv.h>

#include "resident_senses.h"
#include "ears.h"
#include "stream.h"
#include "score_view.h"
#include "tracker_view.h"
#include "midi.h"
#include "midi_view.h"
#include "jam.h"
#include "synth_view.h"
#include "tr808_view.h"
#include "tr808_audio.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace sound {

static Host g_host;
static void wakeApp() { TEventQueue::wakeUp(); }

void setHost(const Host& h) {
    g_host = h;
    startStream();
    midi::setWake(&wakeApp);
    midi::start();
}
const Host& host() { return g_host; }

double clockMs() {
    // One clock for every sound: the stream's samples while it runs (stream.cpp), else steady time.
    if (streamRunning()) return streamClockMs();
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
bool synthAudioAvailable() { return std::string(streamOut()) == "coreaudio" || (!streamRunning() && tr808::playerAvailable()); }

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

bool sendToSpeakers(const std::vector<Note>& notes, double nowMs) {
    if (!synthAudioEnabled() || notes.empty()) return false;
    if (streamRunning()) {
        // Into the one stream, each note at its own sample: in time, sharing the speaker and the FM channels.
        const int64_t base = sampleForClock(nowMs);
        for (const auto& n : notes) mixer().schedule(n, base + msToSample(n.atMs));
        return true;
    }
    if (!tr808::playerAvailable()) return false;   // no stream (WWDOS_SOUND_OUT=off): one afplay per phrase
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
    TWindow* w = createSynthWindow(boundsFrom(kv, 68, 18), kv);
    const std::string id = g_host.insert ? g_host.insert(w) : "";
    return "{\"ok\":true,\"id\":\"" + id + "\"}";
}

void poll() {
    jamTick();
    auto msgs = midi::takeIn();
    if (msgs.empty()) return;
    for (const auto& m : msgs) {
        if (m.bytes.size() < 3) continue;
        const int kind = m.bytes[0] & 0xF0;
        if (kind != 0x90 && kind != 0x80) continue;
        const bool on = kind == 0x90 && m.bytes[2] > 0;
        auto* w = frontMost<TSynthWindow>();
        if (!w) {
            if (!on) continue;
            openSynth({});
            w = frontMost<TSynthWindow>();
            if (!w) return;
        }
        // A MIDI keyboard is the person's hands: its notes are the person's, from the console.
        resident::WhoScope as(resident::personWho());
        w->view()->midiNote(m.bytes[1], m.bytes[2], on, "person");
        if (on)
            resident::addAct(resident::personWho(), "midi " + midi::describe(m.bytes) + " from " + m.from,
                             g_host.idOf ? g_host.idOf(w) : "", "");
    }
}

bool handles(const std::string& name) {
    return name == "open_synth" || name == "synth" || name == "hear" || name == "open_event_score" ||
           name == "event_score" || name == "sound" || name == "open_tracker" || name == "tracker" ||
           name == "open_midi" || name == "midi" || name == "jam";
}

// scores/<name>.score: $WWDOS_REPO/scores, ./scores, then the source tree.
static bool readScoreFile(const std::string& name, std::string& text, std::string& origin) {
    std::vector<std::string> dirs;
    if (const char* r = std::getenv("WWDOS_REPO")) if (*r) dirs.push_back(std::string(r) + "/scores");
    dirs.push_back("scores");
#ifdef WWDOS_SOURCE_DIR
    dirs.push_back(std::string(WWDOS_SOURCE_DIR) + "/scores");
#endif
    for (const auto& d : dirs) {
        const std::string p = d + "/" + name + ".score";
        FILE* f = std::fopen(p.c_str(), "rb");
        if (!f) continue;
        char buf[4096];
        size_t n;
        text.clear();
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && text.size() < 65536) text.append(buf, n);
        std::fclose(f);
        origin = "scores/" + name + ".score";
        return true;
    }
    return false;
}

// tracks/<name>.trk: the same places as scores. Writes go to the first that exists (else ./tracks).
static std::vector<std::string> trackDirs() {
    std::vector<std::string> dirs;
    if (const char* r = std::getenv("WWDOS_REPO")) if (*r) dirs.push_back(std::string(r) + "/tracks");
    dirs.push_back("tracks");
#ifdef WWDOS_SOURCE_DIR
    dirs.push_back(std::string(WWDOS_SOURCE_DIR) + "/tracks");
#endif
    return dirs;
}

static bool validName(const std::string& n) {
    if (n.empty() || n.size() > 40) return false;
    for (char c : n) if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_')) return false;
    return true;
}

static bool readTrack(const std::string& name, std::string& text, std::string& origin) {
    for (const auto& d : trackDirs()) {
        FILE* f = std::fopen((d + "/" + name + ".trk").c_str(), "rb");
        if (!f) continue;
        char buf[4096];
        size_t n;
        text.clear();
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && text.size() < 65536) text.append(buf, n);
        std::fclose(f);
        origin = "tracks/" + name + ".trk";
        return true;
    }
    return false;
}

static bool writeTrack(const std::string& name, const std::string& text, std::string& where) {
    std::string dir;
    struct stat st;
    for (const auto& d : trackDirs()) if (::stat(d.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) { dir = d; break; }
    if (dir.empty()) { dir = "tracks"; ::mkdir(dir.c_str(), 0755); }
    where = dir + "/" + name + ".trk";
    FILE* f = std::fopen(where.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    std::fclose(f);
    return ok;
}

static std::string openTracker(const KV& kv) {
    Song song;
    std::string origin = "new";
    if (kvHas(kv, "track") || kvHas(kv, "text")) {
        std::string text;
        if (kvHas(kv, "track")) {
            if (!validName(kvGet(kv, "track"))) return "err track is letters, digits, - and _";
            if (!readTrack(kvGet(kv, "track"), text, origin)) return "err no track tracks/" + kvGet(kv, "track") + ".trk";
        } else {
            text = kvGet(kv, "text");
            origin = "text";
        }
        std::string err;
        if (!parseSong(text, song, err)) return "err song " + err;
    } else {
        std::vector<TrackChannel> chans;
        std::string spec = kvGet(kv, "channels", "fm:bass fm:lead speaker drums");
        for (char& c : spec) if (c == ',') c = ' ';
        std::istringstream is(spec);
        for (std::string w; is >> w;) {
            TrackChannel ch;
            if (!parseChan(w, ch)) return "err a channel is fm:<patch>, speaker or drums: " + w;
            chans.push_back(ch);
        }
        if (chans.empty() || chans.size() > 12) return "err 1 to 12 channels";
        song = newSong(toInt(kvGet(kv, "rows"), 16), chans);
        if (kvHas(kv, "tempo")) song.tempo = std::max(30, std::min(300, toInt(kvGet(kv, "tempo"), 120)));
    }
    const int w = std::min(160, 6 + 13 * (int)song.chans.size());
    TWindow* win = new TTrackerWindow(boundsFrom(kv, std::max(60, w), std::min(36, song.rows() + 5)), song, origin);
    const std::string id = g_host.insert ? g_host.insert(win) : "";
    if (truthy(kvGet(kv, "play"))) static_cast<TTrackerWindow*>(win)->view()->play(0);
    return "{\"ok\":true,\"id\":\"" + id + "\",\"title\":\"" + jsonEscape(song.title) + "\",\"rows\":" +
           std::to_string(song.rows()) + ",\"channels\":" + std::to_string(song.chans.size()) + "}";
}

static std::string openScore(const KV& kv) {
    std::string text, origin;
    // score= (exec_command's name= is the command's own name)
    if (kvHas(kv, "score")) {
        const std::string name = kvGet(kv, "score");
        for (char c : name)
            if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_')) return "err name is letters, digits, - and _";
        if (!readScoreFile(name, text, origin)) return "err no score scores/" + name + ".score";
    } else if (kvHas(kv, "text")) {
        text = kvGet(kv, "text");
        origin = "text";
    } else if (!readScoreFile("call-and-answer", text, origin)) {
        return "err open_event_score needs score=<name> (scores/<name>.score) or text=";
    }   // neither given: the worked example
    Score s;
    std::string err;
    if (!parseScore(text, s, err)) return "err score " + err;
    TWindow* w = new TScoreWindow(boundsFrom(kv, 78, std::min(30, (int)s.sections.size() + 9)), s, origin);
    const std::string id = g_host.insert ? g_host.insert(w) : "";
    if (truthy(kvGet(kv, "start"))) static_cast<TScoreWindow*>(w)->view()->start();
    return "{\"ok\":true,\"id\":\"" + id + "\",\"title\":\"" + jsonEscape(s.title) + "\",\"sections\":" +
           std::to_string(s.sections.size()) + "}";
}

std::string command(const std::string& name, const KV& kv) {
    if (name == "open_synth") return openSynth(kv);
    if (name == "sound")
        return streamCommand(kvGet(kv, "action", "state"), kvHas(kv, "ms") ? std::atof(kvGet(kv, "ms").c_str()) : 1000.0);
    if (name == "open_event_score") return openScore(kv);
    if (name == "open_tracker") return openTracker(kv);
    if (name == "jam") return jamCommand(kv);
    if (name == "open_midi") {
        TWindow* w = new TMidiWindow(boundsFrom(kv, 64, 16));
        const std::string id = g_host.insert ? g_host.insert(w) : "";
        return "{\"ok\":true,\"id\":\"" + id + "\"}";
    }
    if (name == "midi") {
        const std::string action = kvGet(kv, "action", "state");
        if (action == "in" || action == "out") {
            std::vector<uint8_t> b;
            if (!midi::parseHex(kvGet(kv, "data"), b)) return "err data is hex bytes, as 90 3C 64";
            if (action == "in") { midi::inject(b, "test"); poll(); }
            else midi::send(b, clockMs(), currentActor());
        } else if (action == "clock") {
            midi::setClockOut(truthy(kvGet(kv, "on", "1")));
        } else if (action != "state" && action != "log") {
            return "err action is state|log|in|out|clock";
        }
        std::ostringstream o;
        o << "{\"ok\":true,\"standing\":\"data\",\"device\":\"" << midi::device() << "\",\"sources\":" << midi::sourcesConnected()
          << ",\"in\":" << midi::inCount() << ",\"out\":" << midi::outCount() << ",\"clock_out\":" << (midi::clockOut() ? "true" : "false");
        if (action == "log") {
            o << ",\"log\":[";
            auto r = midi::recent((size_t)std::max(1, std::min(1024, toInt(kvGet(kv, "n"), 100))));
            for (size_t i = 0; i < r.size(); ++i) {
                char ms[32];
                std::snprintf(ms, sizeof(ms), "%.3f", r[i].ms);   // fixed: the default precision drops the decimals after 10 s
                o << (i ? "," : "") << "{\"ms\":" << ms << ",\"dir\":\"" << (r[i].out ? "out" : "in") << "\",\"from\":\""
                  << jsonEscape(r[i].from) << "\",\"hex\":\"" << midi::hex(r[i].bytes) << "\",\"text\":\""
                  << jsonEscape(midi::describe(r[i].bytes)) << "\"}";
            }
            o << "]";
        }
        o << "}";
        return o.str();
    }
    if (name == "tracker") {
        const std::string action = kvGet(kv, "action");
        auto* w = findOrFront<TTrackerWindow>(kv);
        if (!w) {
            if (kvHas(kv, "id")) return "err tracker window not found";
            if (action != "open") return "err no tracker window (open_tracker opens one)";
            openTracker({});
            w = frontMost<TTrackerWindow>();
            if (!w) return "err tracker spawn failed";
        }
        if (action == "save") {
            const std::string n = kvGet(kv, "track");
            if (!validName(n)) return "err track is letters, digits, - and _";
            std::string where;
            if (!writeTrack(n, songText(w->view()->song()), where)) return "err could not write " + where;
            return withId(w, "{\"ok\":true,\"saved\":\"tracks/" + n + ".trk\"}");
        }
        if (action == "load") {
            KV k2 = kv;
            if (kvHas(kv, "track")) {
                const std::string n = kvGet(kv, "track");
                if (!validName(n)) return "err track is letters, digits, - and _";
                std::string text, origin;
                if (!readTrack(n, text, origin)) return "err no track tracks/" + n + ".trk";
                k2["text"] = text;
                k2["from"] = origin;
            }
            return withId(w, w->view()->apiAction("load_text", k2));
        }
        if (action == "load_text") return "err use action=load text=";
        return withId(w, w->view()->apiAction(action, kv));
    }
    if (name == "event_score") {
        auto* w = findOrFront<TScoreWindow>(kv);
        if (!w) return kvHas(kv, "id") ? "err score window not found" : "err no score window (open_event_score opens one)";
        return withId(w, w->view()->apiAction(kvGet(kv, "action"), kv));
    }
    if (name == "hear") {
        // of= (not source=, which names the caller's standing)
        const std::string src = kvGet(kv, "of", "all");
        if (src != "all" && src != "synth" && src != "tr808") return "err of is all|synth|tr808";
        const double secs = kvHas(kv, "seconds") ? std::atof(kvGet(kv, "seconds").c_str()) : 4.0;
        std::string rep = ears().report(clockMs(), secs, src == "all" ? "" : src, toInt(kvGet(kv, "row_ms"), 125),
                                        kvGet(kv, "window"));
        // A tracker's window: the rows as the tracker prints them, on its own row grid.
        if (kvHas(kv, "window") && g_host.find)
            if (auto* tw = dynamic_cast<TTrackerWindow*>(g_host.find(kvGet(kv, "window")))) {
                const TTrackerView* v = tw->view();
                const double origin = v->startedMs() + leadMs();
                auto cr = ears().cellRows(clockMs(), std::max(0.05, std::min(120.0, secs)) * 1000.0, rowMs(v->song()), origin,
                                          kvGet(kv, "window"), v->song().rows());
                std::string arr = "[";
                for (size_t i = 0; i < cr.size(); ++i) arr += (i ? ",\"" : "\"") + jsonEscape(cr[i]) + "\"";
                arr += "]";
                rep = rep.substr(0, rep.size() - 1) + ",\"tracker_rows\":" + arr + "}";
            }
        return rep;
    }
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

}  // namespace sound

// The registry's entry point (declared weak in command_registry.cpp).
std::string sound_registry_command(const std::string& name, const std::map<std::string, std::string>& kv,
                                   bool* handled) {
    *handled = sound::handles(name);
    return *handled ? sound::command(name, kv) : std::string();
}
