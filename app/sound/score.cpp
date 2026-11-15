#include "score.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace sound {

static std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

static bool isName(const std::string& s) {
    if (s.empty() || s.size() > 24) return false;
    for (char c : s) if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_')) return false;
    return true;
}

static bool isInt(const std::string& s) {
    if (s.empty() || s.size() > 6) return false;
    for (char c : s) if (!std::isdigit((unsigned char)c)) return false;
    return true;
}

static bool parseAct(const std::vector<std::string>& t, ScoreAct& a, std::string& why) {
    // t[0] == "do"
    if (t.size() < 3) { why = "do needs a target and a verb (do tr808 play)"; return false; }
    a.target = lower(t[1]);
    a.verb = lower(t[2]);
    a.arg = t.size() > 3 ? lower(t[3]) : "";
    if (a.target != "tr808" && a.target != "synth") { why = "do acts on tr808 or synth only"; return false; }
    if (t.size() > 4) { why = "do takes at most one argument"; return false; }
    if (a.target == "tr808") {
        if ((a.verb == "play" || a.verb == "stop") && a.arg.empty()) return true;
        if (a.verb == "preset" && isName(a.arg)) return true;
        if (a.verb == "tempo" && isInt(a.arg)) return true;
        why = "tr808 does play, stop, preset <name>, tempo <bpm>";
        return false;
    }
    if (a.target == "synth") {
        if ((a.verb == "voice" || a.verb == "patch") && isName(a.arg)) return true;
        why = "synth does voice <pcspk|fm>, patch <name>";
        return false;
    }
    why = "do acts on tr808 or synth only";
    return false;
}

bool parseScore(const std::string& text, Score& out, std::string& err) {
    out = Score();
    std::istringstream is(text);
    std::string raw;
    int n = 0;
    auto fail = [&](const std::string& why) {
        err = "line " + std::to_string(n) + ": " + why;
        return false;
    };
    while (std::getline(is, raw)) {
        ++n;
        if (n > 400) return fail("a score has at most 400 lines");
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::vector<std::string> t;
        for (std::string w; ls >> w;) t.push_back(w);
        const std::string k = lower(t[0]);
        auto rest = [&](size_t from) {
            size_t pos = 0;
            for (size_t i = 0; i < from && pos != std::string::npos; ++i) {
                pos = line.find_first_not_of(" \t", pos);
                pos = line.find_first_of(" \t", pos);
            }
            return pos == std::string::npos ? std::string() : trim(line.substr(pos));
        };
        if (k == "title") {
            out.title = rest(1).substr(0, 60);
        } else if (k == "tempo") {
            if (t.size() != 2 || !isInt(t[1])) return fail("tempo takes a number of beats per minute");
            out.tempo = std::atoi(t[1].c_str());
            if (out.tempo < 30 || out.tempo > 600) return fail("tempo is 30 to 600");
        } else if (k == "beats") {
            if (t.size() != 2 || !isInt(t[1])) return fail("beats takes the beats in a bar");
            out.beatsPerBar = std::atoi(t[1].c_str());
            if (out.beatsPerBar < 1 || out.beatsPerBar > 16) return fail("beats is 1 to 16");
        } else if (k == "part") {
            if (t.size() < 2 || !isName(t[1])) return fail("part takes a name (letters, digits, - and _)");
            out.parts.push_back({lower(t[1]), rest(2).substr(0, 80)});
        } else if (k == "do") {
            if (out.sections.empty()) return fail("a do line follows the section it starts with");
            ScoreAct a;
            std::string why;
            if (!parseAct(t, a, why)) return fail(why);
            if (out.sections.back().acts.size() >= 8) return fail("at most 8 do lines per section");
            out.sections.back().acts.push_back(a);
        } else if (isInt(t[0])) {
            if (t.size() < 3) return fail("a section is: <bars> <who> <instruction>");
            ScoreSection s;
            s.bars = std::atoi(t[0].c_str());
            if (s.bars < 1 || s.bars > 64) return fail("a section is 1 to 64 bars");
            s.who = lower(t[1]);
            if (!isName(s.who)) return fail("who is a part name or all");
            bool known = s.who == "all";
            for (const auto& p : out.parts) if (p.first == s.who) known = true;
            if (!known) return fail("no part named " + s.who + " (add a part line before the sections)");
            s.text = rest(2).substr(0, 120);
            const std::string lt = lower(s.text);
            s.kind = lt.rfind("rest", 0) == 0 ? "rest" : lt.find("answer") != std::string::npos ? "answer" : "open";
            s.line = n;
            if (out.sections.size() >= 64) return fail("at most 64 sections");
            out.sections.push_back(s);
        } else {
            return fail("unknown line (title, tempo, beats, part, do, or a section starting with its bars)");
        }
    }
    if (out.sections.empty()) { n = 0; return fail("a score needs at least one section"); }
    if (out.title.empty()) out.title = "untitled";
    err.clear();
    return true;
}

double barMs(const Score& s) { return s.beatsPerBar * 60000.0 / s.tempo; }

double sectionStartMs(const Score& s, int i) {
    double ms = 0;
    for (int k = 0; k < i && k < (int)s.sections.size(); ++k) ms += s.sections[k].bars * barMs(s);
    return ms;
}

double sectionMs(const Score& s, int i) {
    return (i >= 0 && i < (int)s.sections.size()) ? s.sections[i].bars * barMs(s) : 0;
}

double totalMs(const Score& s) { return sectionStartMs(s, (int)s.sections.size()); }

ScorePos positionAt(const Score& s, double t) {
    ScorePos p;
    if (t < 0) t = 0;
    double start = 0;
    for (int i = 0; i < (int)s.sections.size(); ++i) {
        const double len = sectionMs(s, i);
        if (t < start + len) {
            p.section = i;
            p.intoMs = t - start;
            p.leftMs = len - p.intoMs;
            const double beatMs = 60000.0 / s.tempo;
            const int beats = (int)std::floor(p.intoMs / beatMs);
            p.bar = beats / s.beatsPerBar;
            p.beat = beats % s.beatsPerBar;
            return p;
        }
        start += len;
    }
    p.section = (int)s.sections.size();
    p.done = true;
    return p;
}

}  // namespace sound
