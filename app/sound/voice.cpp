#include "voice.h"

#include "synth.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace sound {
namespace voice {

static std::mutex g_mu;
static long g_renders = 0;

std::string sayPath() {
    if (const char* e = std::getenv("WWDOS_SAY")) if (*e) return e;
    return "/usr/bin/say";
}

long renders() { std::lock_guard<std::mutex> lk(g_mu); return g_renders; }

bool validWord(const std::string& w) {
    if (w.empty() || w.size() > 24) return false;
    bool letter = false;
    for (char c : w) {
        if (std::isalpha((unsigned char)c)) letter = true;
        else if (c != '\'' && c != '-' && c != ' ') return false;
    }
    return letter;
}

// The voices 'say' has, read once ("say -v ?": one voice a line, its name before the locale).
static const std::vector<std::string>& installed() {
    static std::vector<std::string> names = [] {
        std::vector<std::string> out;
        const std::string cmd = "'" + sayPath() + "' -v '?' 2>/dev/null";
        if (FILE* p = popen(cmd.c_str(), "r")) {
            char buf[512];
            while (fgets(buf, sizeof buf, p)) {
                // "Eddy (English (UK)) en_GB    # Hello!": the name, then the locale, then a sample line
                std::string l = buf;
                l = l.substr(0, l.find('#'));
                while (!l.empty() && std::isspace((unsigned char)l.back())) l.pop_back();
                size_t k = l.find_last_of(' ');
                if (k == std::string::npos) continue;
                std::string name = l.substr(0, k);
                while (!name.empty() && name.back() == ' ') name.pop_back();
                if (!name.empty()) out.push_back(name);
            }
            pclose(p);
        }
        return out;
    }();
    return names;
}

Singer singer(const std::string& who0) {
    const std::string who = who0 == "wob" ? "wob" : "wib";
    static const std::vector<std::string> wibVoices = {"Sandy (English (UK))", "Flo (English (UK))", "Kate", "Serena", "Daniel"};
    static const std::vector<std::string> wobVoices = {"Grandpa (English (UK))", "Reed (English (UK))", "Daniel", "Eddy (English (UK))", "Oliver"};
    const auto& want = who == "wob" ? wobVoices : wibVoices;
    Singer s;
    s.who = who;
    s.gain = who == "wob" ? 1.45 : 1.0;   // Grandpa runs under Sandy (voices.py's loudness match)
    const auto& have = installed();
    for (size_t i = 0; i < want.size(); ++i)
        if (std::find(have.begin(), have.end(), want[i]) != have.end()) {
            s.voice = want[i];
            s.fallback = i > 0;
            return s;
        }
    s.voice = want[0];   // none found: ask for the house voice and let 'say' choose
    s.fallback = true;
    return s;
}

static std::vector<int16_t> readWav(const std::string& path) {
    std::vector<int16_t> out;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return out;
    unsigned char h[12];
    if (std::fread(h, 1, 12, f) != 12 || std::memcmp(h, "RIFF", 4) || std::memcmp(h + 8, "WAVE", 4)) { std::fclose(f); return out; }
    int channels = 1, bits = 16;
    unsigned char ck[8];
    while (std::fread(ck, 1, 8, f) == 8) {
        const uint32_t len = ck[4] | (ck[5] << 8) | (ck[6] << 16) | ((uint32_t)ck[7] << 24);
        if (!std::memcmp(ck, "fmt ", 4)) {
            std::vector<unsigned char> fm(len);
            if (std::fread(fm.data(), 1, len, f) != len) break;
            if (len >= 16) { channels = fm[2] | (fm[3] << 8); bits = fm[14] | (fm[15] << 8); }
        } else if (!std::memcmp(ck, "data", 4)) {
            if (bits != 16 || channels < 1) break;
            std::vector<int16_t> raw(len / 2);
            raw.resize(std::fread(raw.data(), 2, raw.size(), f));
            for (size_t i = 0; i + (size_t)channels <= raw.size(); i += (size_t)channels) out.push_back(raw[i]);
            break;
        } else {
            std::fseek(f, (long)len + (len & 1), SEEK_CUR);
        }
    }
    std::fclose(f);
    return out;
}

static std::string cacheDir() {
    static const std::string d = [] {
        const char* t = std::getenv("TMPDIR");
        std::string base = (t && *t) ? t : "/tmp";
        if (!base.empty() && base.back() == '/') base.pop_back();
        std::string dir = base + "/wwdos-voice";
        ::mkdir(dir.c_str(), 0700);
        return dir;
    }();
    return d;
}

static uint64_t fnvStr(const std::string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    return h;
}

// Runs say (no shell: the word is one argument) to a 16-bit 22050 Hz WAV.
static bool runSay(const std::string& voiceName, const std::string& word, const std::string& out, std::string& err) {
    const std::string sayp = sayPath();
    const std::string fmt = "--file-format=WAVE";
    const std::string data = "--data-format=LEI16@22050";
    const char* argv[] = {sayp.c_str(), "-v", voiceName.c_str(), "-o", out.c_str(), fmt.c_str(), data.c_str(), word.c_str(), nullptr};
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    // The app blocks and ignores some signals; 'say' stalls mid-file with them, so it starts
    // with an empty mask and every signal at its default.
    posix_spawnattr_t at;
    posix_spawnattr_init(&at);
    sigset_t none, all;
    sigemptyset(&none);
    sigfillset(&all);
    posix_spawnattr_setsigmask(&at, &none);
    posix_spawnattr_setsigdefault(&at, &all);
    posix_spawnattr_setflags(&at, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
    pid_t pid = 0;
    const int rc = posix_spawn(&pid, sayp.c_str(), &fa, &at, const_cast<char* const*>(argv), environ);
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&at);
    if (rc != 0) { err = "cannot run " + sayp; return false; }
    int st = 0;
    pid_t r;
    while ((r = ::waitpid(pid, &st, 0)) < 0 && errno == EINTR) {}
    if (r < 0) {
        // The app may ignore SIGCHLD, and then children are reaped for it and waitpid has no
        // status to give: wait for the process to go, then judge by the file.
        while (::kill(pid, 0) == 0) ::usleep(5000);
        struct stat fs;
        if (::stat(out.c_str(), &fs) != 0 || fs.st_size <= 44) { err = sayp + " wrote nothing for \"" + word + "\""; return false; }
        return true;
    }
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { err = sayp + " failed for \"" + word + "\""; return false; }
    return true;
}

// True once the WAV is whole: its RIFF length matches the file's size. 'say' can return before
// the speech service has finished writing the file.
static bool wavComplete(const std::string& path) {
    struct stat fs;
    if (::stat(path.c_str(), &fs) != 0 || fs.st_size <= 44) return false;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    unsigned char h[8];
    const bool ok = std::fread(h, 1, 8, f) == 8 && !std::memcmp(h, "RIFF", 4) &&
                    (long long)(h[4] | (h[5] << 8) | (h[6] << 16) | ((uint32_t)h[7] << 24)) + 8 == (long long)fs.st_size;
    std::fclose(f);
    return ok;
}

Pcm spoken(const std::string& who, const std::string& word, std::string* err) {
    if (!validWord(word)) { if (err) *err = "a word is letters, ' - and spaces, at most 24"; return nullptr; }
    const Singer s = singer(who);
    const std::string key = s.voice + "|" + word;
    static std::map<std::string, Pcm> cache;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
    }
    char name[40];
    std::snprintf(name, sizeof(name), "/%016llx.wav", (unsigned long long)fnvStr(key + "|" + sayPath()));
    const std::string path = cacheDir() + name;
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) {
        std::string e;
        if (!runSay(s.voice, word, path, e)) { if (err) *err = e; return nullptr; }
        // 'say' can return while the speech service is still writing the file (its header grows
        // with it): wait until the file is whole and has not grown for 100 ms (at most 3 s).
        long long last = -1;
        int still = 0;
        for (int i = 0; i < 150 && still < 5; ++i) {
            struct stat fs;
            const long long sz = ::stat(path.c_str(), &fs) == 0 ? (long long)fs.st_size : -1;
            still = (sz == last && wavComplete(path)) ? still + 1 : 0;
            last = sz;
            ::usleep(20000);
        }
        std::lock_guard<std::mutex> lk(g_mu);
        ++g_renders;
    }
    auto pcm = std::make_shared<std::vector<int16_t>>(readWav(path));
    if (pcm->empty()) {
        // Seen when the app runs under a tmux server started outside the login (GUI) session:
        // 'say' exits 0 having written only a header.
        if (err) *err = "say wrote no speech for \"" + word + "\" (is the app outside the GUI session, under a detached tmux?)";
        ::unlink(path.c_str());
        return nullptr;
    }
    // trim the silence 'say' leaves before and after the word
    size_t a = 0, b = pcm->size();
    while (a < b && std::abs((int)(*pcm)[a]) < 300) ++a;
    while (b > a && std::abs((int)(*pcm)[b - 1]) < 300) --b;
    auto trimmed = std::make_shared<std::vector<int16_t>>(pcm->begin() + (long)a, pcm->begin() + (long)b);
    std::lock_guard<std::mutex> lk(g_mu);
    cache[key] = trimmed;
    return trimmed;
}

double pitchOf(const std::vector<int16_t>& pcm) {
    const int frame = 1024, hop = 512;
    const int minLag = kRate / 500, maxLag = kRate / 70;
    std::vector<double> f0s;
    for (size_t at = 0; at + frame + maxLag < pcm.size(); at += hop) {
        double energy = 0;
        for (int i = 0; i < frame; ++i) energy += (double)pcm[at + i] * pcm[at + i];
        if (energy / frame < 500.0 * 500.0) continue;   // too quiet to be voiced
        std::vector<double> rs(maxLag + 2, 0.0);
        double best = 0;
        for (int lag = minLag; lag <= maxLag + 1; ++lag) {
            double num = 0, d1 = 0, d2 = 0;
            for (int i = 0; i < frame; ++i) {
                const double x = pcm[at + i], y = pcm[at + i + lag];
                num += x * y;
                d1 += x * x;
                d2 += y * y;
            }
            rs[lag] = num / std::sqrt(d1 * d2 + 1e-9);
            if (lag <= maxLag) best = std::max(best, rs[lag]);
        }
        if (best < 0.6) continue;
        // The first peak close to the best (an octave below scores as well as the true period),
        // refined between samples by a parabola.
        int bestLag = 0;
        for (int lag = minLag + 1; lag < maxLag; ++lag)
            if (rs[lag] >= 0.9 * best && rs[lag] >= rs[lag - 1] && rs[lag] >= rs[lag + 1]) { bestLag = lag; break; }
        if (!bestLag) continue;
        const double a = rs[bestLag - 1], b = rs[bestLag], c = rs[bestLag + 1];
        const double den = a - 2 * b + c;
        const double lag = bestLag + (std::fabs(den) > 1e-12 ? 0.5 * (a - c) / den : 0.0);
        f0s.push_back((double)kRate / lag);
    }
    if (f0s.empty()) return 0;
    std::sort(f0s.begin(), f0s.end());
    return f0s[f0s.size() / 2];
}

Pcm sung(const std::string& who, const std::string& word, int midi, double durMs, std::string* err) {
    Pcm src = spoken(who, word, err);
    if (!src) return nullptr;
    const std::string key = singer(who).voice + "|" + word + "|" + std::to_string(midi) + "|" + std::to_string((long)durMs);
    static std::map<std::string, Pcm> cache;
    static std::map<const std::vector<int16_t>*, double> pitches;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
    }
    double f0;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = pitches.find(src.get());
        f0 = it != pitches.end() ? it->second : -1;
    }
    if (f0 < 0) {
        f0 = pitchOf(*src);
        std::lock_guard<std::mutex> lk(g_mu);
        pitches[src.get()] = f0;
    }
    // Resample: reading the word faster raises its pitch to the note's (and shortens it).
    const double ratio = f0 > 0 ? noteHz(midi) / f0 : 1.0;
    const size_t maxOut = (size_t)std::max(1.0, durMs * kRate / 1000.0);
    auto out = std::make_shared<std::vector<int16_t>>();
    for (size_t i = 0;; ++i) {
        const double pos = i * ratio;
        const size_t k = (size_t)pos;
        if (k + 1 >= src->size() || out->size() >= maxOut) break;
        const double fr = pos - k;
        out->push_back((int16_t)std::lrint((*src)[k] * (1 - fr) + (*src)[k + 1] * fr));
    }
    const size_t fade = std::min(out->size(), (size_t)(kRate / 100));
    for (size_t i = 0; i < fade; ++i) {
        auto& v = (*out)[out->size() - 1 - i];
        v = (int16_t)std::lrint(v * (double)i / fade);
    }
    std::lock_guard<std::mutex> lk(g_mu);
    cache[key] = out;
    return out;
}

}  // namespace voice
}  // namespace sound
