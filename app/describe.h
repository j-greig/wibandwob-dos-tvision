// The describe contract: every window type says what it is.
//
// A window type's main class (the window, or the view it wraps) derives from
// ww::Describable and answers two questions from its own model, never from the
// cells it drew: one sentence of state (what it is doing now), and the text it
// holds when it holds text (editor lines, chat messages, terminal output). An
// agent reads both with the describe command and never scrapes the screen.
// The audit (tools/describe_audit.py, tests/resident/test_describe.py) lists every
// window class in the app and fails when one has no describer.
// Contract and limits: docs/resident-world.md.
#ifndef WW_DESCRIBE_H
#define WW_DESCRIBE_H

#include <string>
#include <vector>

class TWindow;

namespace ww {

class Describable {
public:
    virtual ~Describable() = default;
    // One sentence of state from the model: what the window is and is doing now.
    virtual std::string describeState() const = 0;
    // The text the window holds in its model. false when its cells are its text.
    virtual bool describeText(std::vector<std::string>& out) const { (void)out; return false; }
};

// The window's describer: the window itself, or the first view inside it (two
// levels deep) that derives from Describable; nullptr when it has none.
Describable* describableOf(TWindow* w);

// A describer kept outside a window class, for classes whose source files another
// build compiles and compares byte for byte (the film engine: frame_animation_window,
// frame_file_player_view, gradient). Register at start; the audit reads className.
struct ExternalDescriber {
    const char* className;
    bool (*matches)(TWindow*);
    std::string (*state)(TWindow*);
    bool (*text)(TWindow*, std::vector<std::string>&);   // may be nullptr: its cells are its text
};
void addExternalDescriber(const ExternalDescriber& d);

// Describe any window: true when it has a describer (a Describable or an external one).
// cls is the describer's C++ class; text is filled from the model and modelText says so.
bool describeWindow(TWindow* w, std::string& state, std::vector<std::string>& text, bool& modelText, std::string& cls);

// A dialog's sentence (dialogs are described by kind, not class): its title and controls.
std::string dialogState(TWindow* w, std::vector<std::string>& text);

// Helpers for describers (inline: side binaries such as paint_tui link these views without describe.cpp).
inline std::string clipText(const std::string& s, size_t n = 40) {
    std::string one = s;
    for (auto& c : one) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    if (one.size() <= n) return one;
    size_t cut = n;
    while (cut > 0 && (((unsigned char)one[cut]) & 0xC0) == 0x80) --cut;   // keep UTF-8 whole
    return one.substr(0, cut) + "...";
}

inline std::vector<std::string> splitText(const std::string& s) {
    std::vector<std::string> out;
    size_t p = 0;
    while (p <= s.size()) {
        size_t nl = s.find('\n', p);
        if (nl == std::string::npos) nl = s.size();
        std::string l = s.substr(p, nl - p);
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
        out.push_back(l);
        p = nl + 1;
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

inline std::string onOff(bool b) { return b ? "on" : "off"; }

}  // namespace ww

#endif
