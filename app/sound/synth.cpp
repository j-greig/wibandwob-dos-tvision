#include "synth.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sound {

static const double kPi = 3.14159265358979323846;

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

static size_t toSample(double ms) { return (size_t)std::floor(ms * kRate / 1000.0 + 0.5); }

std::vector<int16_t> render(const std::vector<Note>& notesIn, double totalMs) {
    const size_t total = toSample(std::max(0.0, totalMs));
    std::vector<double> mix(total, 0.0);

    // Stable order by start time; ties keep the caller's order.
    std::vector<Note> notes = notesIn;
    std::stable_sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) { return a.atMs < b.atMs; });

    // PC speaker: one voice, a note ends when the next one starts.
    std::vector<const Note*> spk;
    for (const auto& n : notes) if (n.voice == Voice::PcSpeaker) spk.push_back(&n);
    for (size_t k = 0; k < spk.size(); ++k) {
        const Note& n = *spk[k];
        size_t a = toSample(n.atMs), b = toSample(n.atMs + n.durMs);
        if (k + 1 < spk.size()) b = std::min(b, toSample(spk[k + 1]->atMs));
        b = std::min(b, total);
        const double hz = speakerHz(n.midi);
        const double amp = speakerPatch().level * 0.5;
        for (size_t i = a; i < b; ++i) {
            const double ph = std::fmod((double)(i - a) * hz / kRate, 1.0);
            mix[i] += ph < 0.5 ? amp : -amp;
        }
    }

    // FM: nine channels; a tenth note at once takes the channel that started first.
    std::vector<const Note*> fm;
    for (const auto& n : notes) if (n.voice == Voice::Fm) fm.push_back(&n);
    std::vector<double> chanEnd(kFmChannels, -1.0), chanStart(kFmChannels, -1.0);
    std::vector<double> cut(fm.size(), 1e300);
    std::vector<int> chanNote(kFmChannels, -1);
    for (size_t k = 0; k < fm.size(); ++k) {
        const Note& n = *fm[k];
        int c = -1;
        for (int i = 0; i < kFmChannels; ++i) if (chanEnd[i] <= n.atMs) { c = i; break; }
        if (c < 0) {
            c = 0;
            for (int i = 1; i < kFmChannels; ++i) if (chanStart[i] < chanStart[c]) c = i;
            if (chanNote[c] >= 0) cut[chanNote[c]] = n.atMs;
        }
        chanStart[c] = n.atMs;
        chanEnd[c] = n.atMs + soundingMs(n);
        chanNote[c] = (int)k;
    }
    for (size_t k = 0; k < fm.size(); ++k) {
        const Note& n = *fm[k];
        const Patch& p = n.patch;
        const double end = std::min(n.atMs + soundingMs(n), cut[k]);
        size_t a = toSample(n.atMs), b = std::min(toSample(end), total);
        const double f = noteHz(n.midi);
        const double vel = std::max(1, std::min(127, n.vel)) / 127.0;
        double prev = 0;
        for (size_t i = a; i < b; ++i) {
            const double t = (double)(i - a) / kRate;
            const double env = envelope(p, t * 1000.0, n.durMs);
            const double mod = std::sin(2 * kPi * f * p.modRatio * t + p.feedback * prev) * p.index * (0.4 + 0.6 * env);
            prev = mod;
            mix[i] += std::sin(2 * kPi * f * p.carRatio * t + mod) * env * vel * p.level * 0.35;
        }
    }

    std::vector<int16_t> out(total);
    for (size_t i = 0; i < total; ++i) {
        double v = mix[i];
        if (v > 1.0) v = 1.0;
        if (v < -1.0) v = -1.0;
        out[i] = (int16_t)std::lrint(v * 32767.0);
    }
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
