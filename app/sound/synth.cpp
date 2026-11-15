#include "synth.h"
#include "mixer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sound {

const char* voiceId(Voice v) { return v == Voice::PcSpeaker ? "pcspk" : "fm"; }

bool parseVoice(const std::string& s0, Voice& out) {
    std::string s;
    for (char c : s0) s += (char)std::tolower((unsigned char)c);
    if (s == "pcspk" || s == "speaker" || s == "pc" || s == "square") { out = Voice::PcSpeaker; return true; }
    if (s == "fm" || s == "adlib" || s == "opl") { out = Voice::Fm; return true; }
    return false;
}

const std::vector<Patch>& fmPatches() {
    static const std::vector<Patch> p = {
        //  name     mod  car  index fb    atk  dec  sus   rel   level
        {"bass",    1.0, 1.0, 2.6, 0.35,  2,  180, 0.45,  60, 1.00},
        {"organ",   2.0, 1.0, 1.2, 0.00,  5,   60, 0.80,  80, 0.80},
        {"bell",    3.5, 1.0, 3.0, 0.00,  1,  900, 0.00, 400, 0.80},
        {"brass",   1.0, 1.0, 1.8, 0.20, 40,  120, 0.70, 100, 0.85},
        {"lead",    2.0, 1.0, 2.2, 0.60,  3,  100, 0.60,  80, 0.80},
        {"pluck",   1.0, 1.0, 3.5, 0.10,  1,  140, 0.00,  50, 0.90},
        {"chip",    1.0, 1.0, 0.9, 0.90,  1,   40, 0.70,  20, 0.70},
    };
    return p;
}

int patchIndex(const std::string& name0) {
    std::string name;
    for (char c : name0) name += (char)std::tolower((unsigned char)c);
    const auto& p = fmPatches();
    for (size_t i = 0; i < p.size(); ++i) if (p[i].name == name) return (int)i;
    return -1;
}

const Patch& speakerPatch() {
    static const Patch p = {"square", 0, 1, 0, 0, 0, 0, 1, 0, 0.55};
    return p;
}

int parseNote(const std::string& s0) {
    std::string s;
    for (char c : s0) if (!std::isspace((unsigned char)c)) s += c;
    if (s.empty()) return -1;
    if (std::isdigit((unsigned char)s[0])) {
        for (char c : s) if (!std::isdigit((unsigned char)c)) return -1;
        int n = std::atoi(s.c_str());
        return (n >= 0 && n <= 127) ? n : -1;
    }
    static const int base[7] = {9, 11, 0, 2, 4, 5, 7};  // a b c d e f g
    char l = (char)std::tolower((unsigned char)s[0]);
    if (l < 'a' || l > 'g') return -1;
    int pc = base[l - 'a'];
    size_t i = 1;
    if (i < s.size() && (s[i] == '#' || s[i] == 's')) { ++pc; ++i; }
    else if (i < s.size() && s[i] == 'b' && i + 1 < s.size()) { --pc; ++i; }
    else if (i < s.size() && s[i] == '-') { ++i; }
    if (i >= s.size()) return -1;
    bool neg = false;
    if (s[i] == '-') { neg = true; ++i; }
    if (i >= s.size()) return -1;
    for (size_t k = i; k < s.size(); ++k) if (!std::isdigit((unsigned char)s[k])) return -1;
    int oct = std::atoi(s.c_str() + i) * (neg ? -1 : 1);
    int n = (oct + 1) * 12 + pc;
    return (n >= 0 && n <= 127) ? n : -1;
}

std::string noteName(int midi) {
    static const char* names[12] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
    if (midi < 0 || midi > 127) return "---";
    return std::string(names[midi % 12]) + std::to_string(midi / 12 - 1);
}

double noteHz(int midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }

unsigned speakerDivisor(int midi) {
    double d = std::floor(kPitHz / noteHz(midi) + 0.5);
    if (d < 1) d = 1;
    if (d > 65535) d = 65535;
    return (unsigned)d;
}

double speakerHz(int midi) { return kPitHz / speakerDivisor(midi); }

double envelope(const Patch& p, double t, double dur) {
    if (t < 0) return 0;
    auto held = [&](double tt) {
        if (tt < p.attackMs) return p.attackMs > 0 ? tt / p.attackMs : 1.0;
        tt -= p.attackMs;
        if (tt < p.decayMs) return 1.0 - (1.0 - p.sustain) * (tt / p.decayMs);
        return p.sustain;
    };
    if (t < dur) return held(t);
    const double atRelease = held(dur);
    if (p.releaseMs <= 0) return 0;
    const double r = (t - dur) / p.releaseMs;
    return r >= 1 ? 0 : atRelease * (1.0 - r);
}

double soundingMs(const Note& n) {
    return n.voice == Voice::PcSpeaker ? n.durMs : n.durMs + n.patch.releaseMs;
}

double renderLengthMs(const std::vector<Note>& notes) {
    double end = 0;
    for (const auto& n : notes) end = std::max(end, n.atMs + soundingMs(n));
    return end;
}

static int64_t toSample(double ms) { return msToSample(ms); }

std::vector<int16_t> render(const std::vector<Note>& notes, double totalMs) {
    // The same mixer the live stream uses (mixer.cpp), rendered in one block.
    Mixer m;
    for (const auto& n : notes) m.schedule(n, toSample(n.atMs));
    std::vector<int16_t> out(toSample(std::max(0.0, totalMs)));
    m.render(out.data(), out.size());
    return out;
}

bool writeWav(const std::string& path, const std::vector<int16_t>& s) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    auto u32 = [&](uint32_t v) { unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24)}; std::fwrite(b, 1, 4, f); };
    auto u16 = [&](uint16_t v) { unsigned char b[2] = {(unsigned char)v, (unsigned char)(v >> 8)}; std::fwrite(b, 1, 2, f); };
    const uint32_t data = (uint32_t)(s.size() * 2);
    std::fwrite("RIFF", 1, 4, f); u32(36 + data); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1); u32(kRate); u32(kRate * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(data);
    for (int16_t v : s) u16((uint16_t)v);
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    return ok;
}

uint64_t fnv1a(const std::vector<int16_t>& s) {
    uint64_t h = 1469598103934665603ULL;
    for (int16_t v : s) {
        const uint16_t u = (uint16_t)v;
        h ^= (u & 0xff); h *= 1099511628211ULL;
        h ^= (u >> 8); h *= 1099511628211ULL;
    }
    return h;
}

double rms(const std::vector<int16_t>& s, size_t from, size_t to) {
    to = std::min(to, s.size());
    if (from >= to) return 0;
    double acc = 0;
    for (size_t i = from; i < to; ++i) acc += (double)s[i] * s[i];
    return std::sqrt(acc / (double)(to - from)) / 32767.0;
}

int peak(const std::vector<int16_t>& s) {
    int p = 0;
    for (int16_t v : s) p = std::max(p, std::abs((int)v));
    return p;
}

int upCrossings(const std::vector<int16_t>& s, size_t from, size_t to) {
    to = std::min(to, s.size());
    int n = 0;
    for (size_t i = from + 1; i < to; ++i) if (s[i - 1] < 0 && s[i] >= 0) ++n;
    return n;
}

}  // namespace sound
