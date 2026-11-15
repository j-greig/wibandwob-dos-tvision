#include "ears.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

namespace sound {

static std::string esc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c < 0x20) o += ' ';
        else o += (char)c;
    }
    return o;
}

static std::string num(double v, int prec = 2) {
    char b[32];
    std::snprintf(b, sizeof(b), "%.*f", prec, v);
    return b;
}

Ears& ears() {
    static Ears e;
    return e;
}

double drumRingMs(const std::string& v) {
    static const std::map<std::string, double> ring = {
        {"bd", 400}, {"sd", 200}, {"lt", 300}, {"mt", 300}, {"ht", 300}, {"rs", 60}, {"cb", 200},
        {"cp", 200}, {"ma", 60}, {"cl", 60}, {"cy", 900}, {"oh", 500}, {"ch", 60}};
    auto it = ring.find(v);
    return it == ring.end() ? 200 : it->second;
}

void Ears::hear(const Heard& h) {
    // Keep start order: a phrase's later notes may arrive before an earlier hit is due.
    auto it = log_.end();
    while (it != log_.begin() && (it - 1)->startMs > h.startMs) --it;
    log_.insert(it, h);
    while (log_.size() > keep_) log_.pop_front();
}

void Ears::release(const std::string& window, int midi, double atMs) {
    for (auto it = log_.rbegin(); it != log_.rend(); ++it) {
        if (it->source != "synth" || it->window != window || it->midi != midi || !it->held) continue;
        it->held = false;
        it->durMs = std::max(1.0, atMs - it->startMs);
        it->endMs = atMs + (it->hasEnvelope ? it->envPatch.releaseMs : 0);
        return;
    }
}

double Ears::levelAt(const Heard& h, double t) {
    if (t < h.startMs || t >= h.endMs) return 0;
    if (h.hasEnvelope) return h.level * envelope(h.envPatch, t - h.startMs, h.durMs);
    const double len = std::max(1.0, h.endMs - h.startMs);
    return h.level * (1.0 - (t - h.startMs) / len);
}

static bool match(const Heard& h, const std::string& source) { return source.empty() || source == "all" || h.source == source; }

std::vector<Heard> Ears::sounding(double now, const std::string& source) const {
    std::vector<Heard> out;
    for (const auto& h : log_) if (match(h, source) && h.startMs <= now && h.endMs > now) out.push_back(h);
    return out;
}

std::vector<Heard> Ears::since(double now, double ms, const std::string& source) const {
    std::vector<Heard> out;
    for (const auto& h : log_) if (match(h, source) && h.startMs >= now - ms && h.startMs <= now) out.push_back(h);
    return out;
}

std::string Ears::meter(double now, double ms, double rowMs, const std::string& source) const {
    static const char* bars[9] = {" ", "\u2581", "\u2582", "\u2583", "\u2584", "\u2585", "\u2586", "\u2587", "\u2588"};
    const int n = std::max(1, (int)std::floor(ms / rowMs + 0.5));
    std::string out;
    for (int i = 0; i < n; ++i) {
        const double t = now - ms + (i + 0.5) * rowMs;
        double lv = 0;
        for (const auto& h : log_) if (match(h, source)) lv += levelAt(h, t);
        lv = std::min(1.0, lv);
        out += bars[(int)std::floor(lv * 8 + 0.5)];
    }
    return out;
}

std::vector<std::string> Ears::rows(double now, double ms, double rowMs, const std::string& source) const {
    const int n = std::max(1, std::min(256, (int)std::floor(ms / rowMs + 0.5)));
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) {
        const double a = now - ms + i * rowMs, b = a + rowMs;
        std::string notes, drums;
        for (const auto& h : log_) {
            if (!match(h, source) || h.startMs < a || h.startMs >= b) continue;
            if (h.source == "synth") notes += (notes.empty() ? "" : " ") + noteName(h.midi);
            else {
                std::string up;
                for (char c : h.voice) up += (char)std::toupper((unsigned char)c);
                drums += (drums.empty() ? "" : " ") + up;
            }
        }
        char head[16];
        std::snprintf(head, sizeof(head), "%.3f", -(now - a) / 1000.0);
        if (notes.empty()) notes = "---";
        if (drums.empty()) drums = "..";
        char line[256];
        std::snprintf(line, sizeof(line), "%-8s %-15s | %s", head, notes.c_str(), drums.c_str());
        out.push_back(line);
    }
    return out;
}

std::vector<std::string> Ears::cellRows(double now, double ms, const std::string& window) const {
    // Rows by the time they started: (rowAtMs, row) -> cells by channel.
    std::map<std::pair<double, int>, std::pair<int, std::map<int, std::string>>> rows;
    for (const auto& h : log_) {
        if (h.window != window || h.channel < 0 || h.row < 0) continue;
        if (h.rowAtMs < now - ms - 1e-6 || h.rowAtMs >= now) continue;
        auto& r = rows[{h.rowAtMs, h.row}];
        r.first = std::max(r.first, h.channels);
        if (!r.second.count(h.channel)) r.second[h.channel] = h.cell;
    }
    std::vector<std::string> out;
    for (const auto& kv : rows) {
        char head[8];
        std::snprintf(head, sizeof(head), "%02d ", kv.first.second);
        std::string line = head;
        for (int c = 0; c < kv.second.first; ++c) {
            if (c) line += " | ";
            auto it = kv.second.second.find(c);
            line += it == kv.second.second.end() ? std::string("--- .. ...") : it->second;
        }
        out.push_back(line);
    }
    if (out.size() > 256) out.erase(out.begin(), out.end() - 256);
    return out;
}

std::string Ears::summary(double now, double ms, const std::string& source) const {
    const auto hs = since(now, ms, source);
    std::ostringstream o;
    const double secs = ms / 1000.0;
    o << "in the last " << num(secs, std::fabs(secs - std::floor(secs + 0.5)) < 1e-9 ? 0 : (ms < 1000 ? 2 : 1)) << " s: ";
    std::vector<int> notes;
    std::map<std::string, int> whoSynth, drumCount;
    std::string drumWho;
    int tempo = 0, hits = 0;
    std::map<std::string, std::string> sungBy;   // who -> the words they sang
    for (const auto& h : hs) {
        if (h.source == "synth") { notes.push_back(h.midi); whoSynth[h.who]++; }
        else if (h.source == "voice") sungBy[h.who] += (sungBy[h.who].empty() ? "" : " ") + h.word;
        else { drumCount[h.voice]++; ++hits; tempo = h.tempo; drumWho = h.who; }
    }
    bool any = false;
    for (const auto& s : sungBy) {
        o << (any ? "; " : "") << s.first << " sang \"" << s.second << "\"";
        any = true;
    }
    if (!notes.empty()) {
        if (any) o << "; ";
        any = true;
        const auto mm = std::minmax_element(notes.begin(), notes.end());
        o << "synth " << notes.size() << " note" << (notes.size() == 1 ? "" : "s") << " ("
          << noteName(*mm.first) << (*mm.first == *mm.second ? "" : " to " + noteName(*mm.second)) << ") by ";
        bool first = true;
        for (const auto& w : whoSynth) { o << (first ? "" : ", ") << w.first << " " << w.second; first = false; }
    }
    if (hits) {
        o << (any ? "; " : "") << "tr808 " << hits << " hits (";
        bool first = true;
        for (const auto& d : drumCount) { o << (first ? "" : ", ") << d.first << " " << d.second; first = false; }
        o << ") at " << tempo << " bpm, started by " << drumWho;
        any = true;
    }
    if (!any) o << "silence";
    const auto now_ = sounding(now, source);
    if (now_.empty()) o << ". Nothing sounds now.";
    else {
        o << ". Sounding now: ";
        for (size_t i = 0; i < now_.size(); ++i) {
            const auto& h = now_[i];
            o << (i ? ", " : "") << (h.source == "synth" ? noteName(h.midi) + " " + h.voice : h.voice) << " (" << h.who << ")";
        }
        o << ".";
    }
    return o.str();
}

std::string Ears::report(double now, double seconds, const std::string& source, double rowMs,
                         const std::string& window) const {
    const double ms = std::max(0.05, std::min(120.0, seconds)) * 1000.0;
    rowMs = std::max(20.0, std::min(2000.0, rowMs));
    Ears sub(keep_);
    const Ears* e = this;
    if (!window.empty()) {
        for (const auto& h : log_) if (h.window == window) sub.log_.push_back(h);
        e = &sub;
    }
    std::ostringstream o;
    o << "{\"standing\":\"data\",\"now_ms\":" << (long long)now << ",\"seconds\":" << num(ms / 1000.0, 2)
      << ",\"source\":\"" << esc(source.empty() ? "all" : source) << "\",\"row_ms\":" << (long)rowMs;
    o << ",\"sounding\":[";
    const auto sn = e->sounding(now, source);
    for (size_t i = 0; i < sn.size(); ++i) {
        const auto& h = sn[i];
        o << (i ? "," : "") << "{\"source\":\"" << h.source << "\",\"window\":\"" << esc(h.window) << "\",\"voice\":\"" << esc(h.voice) << "\"";
        if (h.source == "synth") o << ",\"note\":\"" << noteName(h.midi) << "\",\"midi\":" << h.midi << ",\"patch\":\"" << esc(h.patch) << "\"";
        else o << ",\"step\":" << h.step;
        o << ",\"level\":" << num(levelAt(h, now)) << ",\"left_ms\":" << (long)(h.endMs - now) << ",\"who\":\"" << esc(h.who) << "\"}";
    }
    o << "],\"heard\":[";
    auto hs = e->since(now, ms, source);
    const size_t cap = 400;
    const size_t skip = hs.size() > cap ? hs.size() - cap : 0;
    for (size_t i = skip; i < hs.size(); ++i) {
        const auto& h = hs[i];
        o << (i > skip ? "," : "") << "{\"ago_ms\":" << (long)(now - h.startMs) << ",\"at_ms\":" << num(h.startMs, 3)
          << ",\"source\":\"" << h.source
          << "\",\"window\":\"" << esc(h.window) << "\",\"voice\":\"" << esc(h.voice) << "\"";
        if (h.source == "voice")
            o << ",\"word\":\"" << esc(h.word) << "\",\"note\":\"" << noteName(h.midi) << "\",\"midi\":" << h.midi
              << ",\"dur_ms\":" << (long)h.durMs;
        else if (h.source == "synth")
            o << ",\"note\":\"" << noteName(h.midi) << "\",\"midi\":" << h.midi << ",\"patch\":\"" << esc(h.patch)
              << "\",\"dur_ms\":" << (long)h.durMs;
        else
            o << ",\"step\":" << h.step << ",\"accent\":" << (h.accent ? "true" : "false") << ",\"tempo\":" << h.tempo;
        o << ",\"level\":" << num(h.level) << ",\"who\":\"" << esc(h.who) << "\"}";
    }
    int synthN = 0, drumN = 0, voiceN = 0;
    for (const auto& h : hs) (h.source == "synth" ? synthN : h.source == "voice" ? voiceN : drumN)++;
    o << "],\"dropped\":" << skip << ",\"counts\":{\"synth\":" << synthN << ",\"tr808\":" << drumN << ",\"voice\":" << voiceN << "}";
    o << ",\"summary\":\"" << esc(e->summary(now, ms, source)) << "\"";
    o << ",\"meter\":\"" << e->meter(now, ms, rowMs, source) << "\",\"rows\":[";
    const auto rs = e->rows(now, ms, rowMs, source);
    for (size_t i = 0; i < rs.size(); ++i) o << (i ? "," : "") << "\"" << esc(rs[i]) << "\"";
    o << "]}";
    return o.str();
}

}  // namespace sound
