// Event scores: a short text score that a person and a resident perform together on
// the synth and the drum machine. Each section has a length in bars, who it is for,
// and an open instruction ("answer the last phrase", "rest four bars"). The machine
// keeps time and shows the instructions; it acts itself only on "do" lines, from a
// fixed list (the TR-808's transport, preset and tempo; the synth's voice and patch).
// Score text is data: nothing in it reaches a model or runs as a command.
// No Turbo Vision dependency. Format and commands: docs/machine-sound.md.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace sound {

struct ScoreAct {
    std::string target;  // "tr808" or "synth"
    std::string verb;    // tr808: play, stop, preset, tempo; synth: voice, patch
    std::string arg;
    std::string text() const { return target + " " + verb + (arg.empty() ? "" : " " + arg); }
};

struct ScoreSection {
    int bars = 1;
    std::string who;     // a part name or "all"
    std::string text;    // the instruction
    std::string kind;    // "rest" (text starts with rest), "answer" (text holds answer), else "open"
    std::vector<ScoreAct> acts;  // run when the section starts
    int line = 0;
};

struct Score {
    std::string title;
    int tempo = 120;
    int beatsPerBar = 4;
    std::vector<std::pair<std::string, std::string>> parts;  // name, description
    std::vector<ScoreSection> sections;
};

// Parses a score. On failure returns false and err is "line N: ...".
bool parseScore(const std::string& text, Score& out, std::string& err);

double barMs(const Score& s);
double sectionStartMs(const Score& s, int i);   // from the start of the score
double sectionMs(const Score& s, int i);
double totalMs(const Score& s);

struct ScorePos {
    int section = 0;   // 0-based; == sections.size() when done
    int bar = 0;       // 0-based bar within the section
    int beat = 0;      // 0-based beat within the bar
    double intoMs = 0; // ms into the section
    double leftMs = 0; // ms left in the section
    bool done = false;
};
ScorePos positionAt(const Score& s, double elapsedMs);

}  // namespace sound
