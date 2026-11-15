// The jam's rule and reply parser (jam.h), apart from the app so sound_test can check them.
#include "jam.h"

#include "synth.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace sound {

std::vector<std::string> jamRule(const std::vector<std::string>& phrase, const std::string& mode) {
    std::vector<int> m;
    for (const auto& n : phrase) { int v = parseNote(n); if (v >= 0) m.push_back(v); }
    std::vector<std::string> out;
    if (m.empty()) return out;
    if (mode == "echo") {
        for (int v : m) out.push_back(noteName(std::min(127, v + 12)));
    } else if (mode == "invert") {
        for (int v : m) out.push_back(noteName(std::max(0, std::min(127, 2 * m[0] - v))));
    } else {   // answer: backwards, a fifth up
        for (auto it = m.rbegin(); it != m.rend(); ++it) out.push_back(noteName(std::min(127, *it + 7)));
    }
    return out;
}

std::vector<std::string> jamParseReply(const std::string& reply) {
    std::vector<std::string> out;
    std::istringstream is(reply);
    std::string line;
    while (std::getline(is, line)) {
        std::istringstream ls(line);
        std::vector<std::string> notes;
        bool ok = true;
        for (std::string w; ls >> w;) {
            if (w == ".") { notes.push_back("."); continue; }
            if (parseNote(w) < 0 || std::isdigit((unsigned char)w[0])) { ok = false; break; }
            notes.push_back(noteName(parseNote(w)));
        }
        if (ok && !notes.empty() && notes.size() <= 16 && std::any_of(notes.begin(), notes.end(), [](const std::string& n) { return n != "."; }))
            return notes;
    }
    return out;
}

}  // namespace sound
